#include "SLDebugger.h"
#include "WCH_Regs.h"
#include <assert.h>
#include <string.h>

#include "build/singlewire.pio.h"

// Setting reset request clears RESUMEACK

static const uint32_t cmd_runprog = 0x00040000;

volatile void busy_wait(int count) {
  volatile int c = count;
  while(c--);
}

template<typename T>
void set_bit(T* base, int i, bool b) {
  int block = i / (sizeof(T) * 8);
  int index = i % (sizeof(T) * 8);
  base[block] &= ~(1 << index);
  base[block] |=  (b << index);
}

template<typename T>
bool get_bit(T* base, int i) {
  int block = i / (sizeof(T) * 8);
  int index = i % (sizeof(T) * 8);
  return (base[block] >> index) & 1;
}

//------------------------------------------------------------------------------

SLDebugger::SLDebugger() {
  memset(saved_regs, 0, sizeof(saved_regs));
  memset(breakpoints, 0, sizeof(breakpoints));
  //memset(patch_bitmap, 0, sizeof(patch_bitmap));
  memset(flash_dirty, 0, sizeof(flash_dirty));
  memset(flash_clean, 0, sizeof(flash_clean));

  swd_pin = -1;
  active_prog = nullptr;
}

//------------------------------------------------------------------------------

void SLDebugger::init_pio(int swd_pin) {
  this->swd_pin = swd_pin;

  gpio_set_drive_strength(swd_pin, GPIO_DRIVE_STRENGTH_12MA);
  gpio_set_slew_rate     (swd_pin, GPIO_SLEW_RATE_FAST);
  gpio_set_function      (swd_pin, GPIO_FUNC_PIO0);

  int sm = 0;
  pio_clear_instruction_memory(pio0);
  uint offset = pio_add_program(pio0, &singlewire_program);

  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap        (&c, offset + singlewire_wrap_target, offset + singlewire_wrap);
  sm_config_set_sideset     (&c, 1, /*optional*/ false, /*pindirs*/ true);
  sm_config_set_out_pins    (&c, swd_pin, 1);
  sm_config_set_in_pins     (&c, swd_pin);
  sm_config_set_set_pins    (&c, swd_pin, 1);
  sm_config_set_sideset_pins(&c, swd_pin);
  sm_config_set_out_shift   (&c, /*shift_right*/ false, /*autopull*/ false, /*pull_threshold*/ 32);
  sm_config_set_in_shift    (&c, /*shift_right*/ false, /*autopush*/ true,  /*push_threshold*/ 32);

  // 125 mhz / 12 = 96 nanoseconds per tick, close enough to 100 ns.
  sm_config_set_clkdiv      (&c, 12);

  pio_sm_init       (pio0, sm, offset, &c);
  pio_sm_set_pins   (pio0, sm, 0);
  pio_sm_set_enabled(pio0, sm, true);
}

//------------------------------------------------------------------------------

void SLDebugger::reset_dbg() {
  memset(saved_regs, 0, sizeof(saved_regs));
  memset(breakpoints, 0, sizeof(breakpoints));
  //memset(patch_bitmap, 0, sizeof(patch_bitmap));
  memset(flash_dirty, 0, sizeof(flash_dirty));
  memset(flash_clean, 0, sizeof(flash_clean));

  active_prog = nullptr;
  pio0->ctrl = 0b000100010001; // Reset pio block

  // Grab pin and send an 8 usec low pulse to reset debug module
  // If we use the sdk functions to do this we get jitter :/
  sio_hw->gpio_clr    = (1 << swd_pin);
  sio_hw->gpio_oe_set = (1 << swd_pin);
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  busy_wait(100); // ~8 usec
  sio_hw->gpio_oe_clr = (1 << swd_pin);
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // Enable debug output pin
  set_dbg(DM_SHDWCFGR, 0x5AA50400);
  set_dbg(DM_CFGR,     0x5AA50400);

  // Reset debug module
  set_dbg(DM_DMCONTROL, 0x00000000);
  set_dbg(DM_DMCONTROL, 0x00000001);
}

//------------------------------------------------------------------------------

bool SLDebugger::sanity() {
  bool ok = true;

  auto dmcontrol = get_dbg(DM_DMCONTROL);
  if (dmcontrol != 0x00000001) {
    printf("sanity() : DMCONTROL was not clean (was 0x%08x, expected 0x00000001)\n", dmcontrol);
    ok = false;
  }

  auto abstractcs = get_dbg(DM_ABSTRACTCS);
  if (abstractcs != 0x08000002) {
    printf("sanity() : ABSTRACTCS was not clean (was 0x%08x, expected 0x08000002)\n");
    ok = false;
  }

  auto autocmd = get_dbg(DM_ABSTRACTAUTO);
  if (autocmd != 0x00000000) {
    printf("sanity() : AUTOCMD was not clean (was 0x%08x, expected 0x00000000)\n");
    ok = false;
  }

  return ok;
}

//------------------------------------------------------------------------------

void SLDebugger::halt() {
  CHECK(sanity());

  if (halted()) return;

  // Halt CPU
  set_dbg(DM_DMCONTROL, 0x80000001);
  while (!halted());
  set_dbg(DM_DMCONTROL, 0x00000001);
  save_regs();

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::unhalt() {
  CHECK(sanity());

  if (!halted()) return;

  load_regs();
  set_dbg(DM_DMCONTROL, 0x40000001);

  // If we hit a breakpoint immediately after unhalting, we hang here...
  //while (halted());

  set_dbg(DM_DMCONTROL, 0x00000001);

  CHECK(sanity());
}

//------------------------------------------------------------------------------

bool SLDebugger::halted() {
  return Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED;
}

bool SLDebugger::busy() {
  return Reg_ABSTRACTCS(get_dbg(DM_ABSTRACTCS)).BUSY;
}

//------------------------------------------------------------------------------

void SLDebugger::lock_flash() {
  CHECK(sanity());

  if (!halted()) {
    printf("Can't lock flash while running\n");
    return;
  }

  auto ctlr = Reg_FLASH_CTLR(get_mem_u32_aligned(ADDR_FLASH_CTLR));
  ctlr.LOCK = true;
  ctlr.FLOCK = true;
  set_mem_u32_aligned(ADDR_FLASH_CTLR, ctlr);

  CHECK(Reg_FLASH_CTLR(get_mem_u32_aligned(ADDR_FLASH_CTLR)).LOCK);
  CHECK(Reg_FLASH_CTLR(get_mem_u32_aligned(ADDR_FLASH_CTLR)).FLOCK);
  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::unlock_flash() {
  CHECK(sanity());

  if (!halted()) {
    printf("Can't unlock flash while running\n");
    return;
  }

  set_mem_u32_aligned(ADDR_FLASH_KEYR,  0x45670123);
  set_mem_u32_aligned(ADDR_FLASH_KEYR,  0xCDEF89AB);
  set_mem_u32_aligned(ADDR_FLASH_MKEYR, 0x45670123);
  set_mem_u32_aligned(ADDR_FLASH_MKEYR, 0xCDEF89AB);

  CHECK(!Reg_FLASH_CTLR(get_mem_u32_aligned(ADDR_FLASH_CTLR)).LOCK);
  CHECK(!Reg_FLASH_CTLR(get_mem_u32_aligned(ADDR_FLASH_CTLR)).FLOCK);
  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::reset_cpu_and_halt() {
  CHECK(sanity());

  // Halt and leave halt request set
  set_dbg(DM_DMCONTROL, 0x80000001);
  while(!halted());

  // Set reset request
  set_dbg(DM_DMCONTROL, 0x80000003);
  while(!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHAVERESET);

  // Clear reset request and hold halt request
  set_dbg(DM_DMCONTROL, 0x80000001);
  while(!halted()); // this busywait seems to be required or we hang

  // Clear HAVERESET
  set_dbg(DM_DMCONTROL, 0x90000001);
  while(Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHAVERESET);

  // Clear halt request
  set_dbg(DM_DMCONTROL, 0x00000001);

  save_regs();

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::step() {
  CHECK(sanity());

  // Write the DCSR register value to be written to the Data0
  // register, here set dcsr.breakm=1, dcsr.stepie=0,
  // dcsr.step=1, dcsr.prv=3 to enable the machine mode to
  // execute ebreak into debug, single step under the interrupt
  // is prohibited, set single step execution.

  Csr_DCSR old_dcsr = get_csr(CSR_DCSR);
  Csr_DCSR new_dcsr = old_dcsr;
  new_dcsr.STEP      = 1;
  new_dcsr.STOPTIME  = 1;
  new_dcsr.STOPCOUNT = 1;
  new_dcsr.STEPIE    = 0;
  new_dcsr.EBREAKU   = 1;
  new_dcsr.EBREAKS   = 1;
  new_dcsr.EBREAKM   = 1;
  set_csr(CSR_DCSR, new_dcsr);

  // Initiate a resume request.
  set_dbg(DM_DMCONTROL, 0x40000001);
  //while (halted());

  set_dbg(DM_DMCONTROL, 0x80000001);
  while (!halted());

  set_csr(CSR_DCSR, old_dcsr);

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::clear_err() {
  set_dbg(DM_ABSTRACTCS, 0x00030000);
}

//------------------------------------------------------------------------------

uint32_t SLDebugger::get_dbg(uint8_t addr) {
  addr = ((~addr) << 1) | 1;
  pio_sm_put_blocking(pio0, 0, addr);
  return pio_sm_get_blocking(pio0, 0);
}

//------------------------------------------------------------------------------

void SLDebugger::set_dbg(uint8_t addr, uint32_t data) {
  addr = ((~addr) << 1) | 0;
  data = ~data;
  pio_sm_put_blocking(pio0, 0, addr);
  pio_sm_put_blocking(pio0, 0, data);
}

//------------------------------------------------------------------------------

uint32_t SLDebugger::get_gpr(int index) {
  CHECK(sanity());

  Reg_COMMAND cmd;
  cmd.REGNO      = 0x1000 | index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(DM_COMMAND, cmd);
  while(busy());
  auto result = get_dbg(DM_DATA0);

  CHECK(sanity());
  return result;
}

//------------------------------------------------------------------------------

void SLDebugger::set_gpr(int index, uint32_t gpr) {

  CHECK(sanity());

  if (index == 16) {
    set_csr(CSR_DPC, gpr);
    return;
  }
  else {
    Reg_COMMAND cmd;
    cmd.REGNO      = 0x1000 | index;
    cmd.WRITE      = 1;
    cmd.TRANSFER   = 1;
    cmd.POSTEXEC   = 0;
    cmd.AARPOSTINC = 0;
    cmd.AARSIZE    = 2;
    cmd.CMDTYPE    = 0;

    set_dbg(DM_DATA0,   gpr);
    set_dbg(DM_COMMAND, cmd);
    while(busy());
  }

  CHECK(sanity());
}

//------------------------------------------------------------------------------

uint32_t SLDebugger::get_csr(int index) {
  CHECK(sanity());
  //assert(halted);

  Reg_COMMAND cmd;
  cmd.REGNO      = index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(DM_COMMAND, cmd);
  while(busy());
  auto result = get_dbg(DM_DATA0);
  CHECK(sanity());
  return result;
}

//------------------------------------------------------------------------------

void SLDebugger::set_csr(int index, uint32_t data) {
  CHECK(sanity());
  //assert(halted);

  Reg_COMMAND cmd;
  cmd.REGNO      = index;
  cmd.WRITE      = 1;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(DM_DATA0, data);
  set_dbg(DM_COMMAND, cmd);
  while(busy());
  CHECK(sanity());
}

//------------------------------------------------------------------------------

uint32_t SLDebugger::get_mem_u32_aligned(uint32_t addr) {
  if (addr & 3) {
    printf("SLDebugger::get_mem_u32_aligned() - Bad address 0x%08x\n", addr);
    return 0;
  }

  CHECK(sanity());

  static uint32_t prog_get_mem[8] = {
    0xe0000537, // lui  a0, 0xE0000
    0x0f852583, // lw   a1, 0x0F8(a0)
    0x0005a583, // lw   a1, 0x000(a1)
    0x0eb52a23, // sw   a1, 0x0F4(a0)
    0x00100073, // ebreak
    0x00100073, // ebreak
    0x00100073, // ebreak
    0x00100073, // ebreak
  };

  load_prog(prog_get_mem);
  set_dbg(DM_DATA0,   0xDEADBEEF);
  set_dbg(DM_DATA1,   addr);
  set_dbg(DM_COMMAND, cmd_runprog);
  while(busy());
  auto result = get_dbg(DM_DATA0);

  CHECK(sanity());
  return result;
}

//------------------------------------------------------------------------------

uint32_t SLDebugger::get_mem_u32_unaligned(uint32_t addr) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  uint32_t buf[2];
  buf[0] = get_mem_u32_aligned(addr_lo);
  buf[1] = get_mem_u32_aligned(addr_hi);

  return *((uint32_t*)((uint8_t*)buf + offset));
}

//------------------------------------------------------------------------------

uint16_t SLDebugger::get_mem_u16(uint32_t addr) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 1) & ~3;

  uint32_t buf[2];

  if (addr_lo == addr_hi) {
    buf[0] = get_mem_u32_aligned(addr_lo);
    buf[1] = get_mem_u32_aligned(addr_hi);
    return *((uint16_t*)((uint8_t*)buf + offset));
  }
  else {
    buf[0] = get_mem_u32_aligned(addr_lo);
    return *((uint16_t*)((uint8_t*)buf + offset));
  }
}

//------------------------------------------------------------------------------

uint8_t SLDebugger::get_mem_u8(uint32_t addr) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;

  uint32_t buf[2];
  buf[0] = get_mem_u32_aligned(addr_lo);
  return *((uint8_t*)buf + offset);
}

//------------------------------------------------------------------------------

void SLDebugger::set_mem_u32_aligned(uint32_t addr, uint32_t data) {
  CHECK(sanity());

  if (addr & 3) printf("SLDebugger::set_mem_u32_aligned - Bad alignment 0x%08x\n", addr);

  static uint32_t prog_put_mem[8] = {
    0xe0000537, // lui  a0, 0xE0000
    0x0f852583, // lw   a1, 0x0F8(a0)
    0x0f452503, // lw   a0, 0x0F4(a0)
    0x00a5a023, // sw   a0, 0x000(a1)
    0x00100073, // ebreak
    0x00100073, // ebreak
    0x00100073, // ebreak
    0x00100073, // ebreak
  };

  load_prog(prog_put_mem);

  set_dbg(DM_DATA0,   data);
  set_dbg(DM_DATA1,   addr);
  set_dbg(DM_COMMAND, cmd_runprog);
  while(busy());

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::set_mem_u32_unaligned(uint32_t addr, uint32_t data) {
  CHECK(sanity());

  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  uint32_t buf[2];
  buf[0] = get_mem_u32_aligned(addr_lo);
  buf[1] = get_mem_u32_aligned(addr_hi);

  *((uint32_t*)((uint8_t*)buf + offset)) = data;

  set_mem_u32_aligned(addr_lo, buf[0]);
  set_mem_u32_aligned(addr_hi, buf[1]);

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::set_mem_u16(uint32_t addr, uint16_t data) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 1) & ~3;

  uint32_t buf[2];

  if (addr_lo == addr_hi) {
    buf[0] = get_mem_u32_aligned(addr_lo);
    *((uint16_t*)((uint8_t*)buf + offset)) = data;
    set_mem_u32_aligned(addr_lo, buf[0]);
  }
  else {
    buf[0] = get_mem_u32_aligned(addr_lo);
    buf[1] = get_mem_u32_aligned(addr_hi);
    *((uint16_t*)((uint8_t*)buf + offset)) = data;
    set_mem_u32_aligned(addr_lo, buf[0]);
    set_mem_u32_aligned(addr_hi, buf[1]);
  }
  return;
}

//------------------------------------------------------------------------------

void SLDebugger::set_mem_u8(uint32_t addr, uint8_t data) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;

  uint32_t buf[1];

  buf[0] = get_mem_u32_aligned(addr_lo);
  *((uint8_t*)buf + offset) = data;
  set_mem_u32_aligned(addr_lo, buf[0]);

  return;
}

//------------------------------------------------------------------------------

void SLDebugger::get_block_aligned(uint32_t addr, void* dst, int size) {
  CHECK(sanity());
  CHECK(halted());
  CHECK((addr & 3) == 0);
  CHECK((size & 3) == 0);

  static uint32_t prog_get_block_aligned[8] = {
    0xe0000537, // lui    a0, 0xE0000
    0x0f852583, // lw     a1, 0x0F8(a0)
    0x0005a583, // lw     a1, 0x000(a1)
    0x0eb52a23, // sw     a1, 0x0F4(a0)
    0x0f852583, // lw     a1, 0x0F8(a0)
    0x00458593, // addi   a1, a1, 4
    0x0eb52c23, // sw     a1, 0x0F8(a0)
    0x00100073, // ebreak
  };

  load_prog(prog_get_block_aligned);
  set_dbg(DM_DATA1, addr);

  uint32_t* cursor = (uint32_t*)dst;
  for (int i = 0; i < size / 4; i++) {
    set_dbg(DM_COMMAND, cmd_runprog);
    while(busy());
    cursor[i] = get_dbg(DM_DATA0);
  }

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::get_block_unaligned(uint32_t addr, void* dst, int size) {
  CHECK(sanity());
  CHECK(halted());

  static uint32_t prog_get_block_unaligned[8] = {
    0xe0000537, // lui    a0, 0xE0000
    0x0f852583, // lw     a1, 0x0F8(a0)
    0x00058583, // lb     a1, 0x000(a1)
    0x0eb52a23, // sw     a1, 0x0F4(a0)
    0x0f852583, // lw     a1, 0x0F8(a0)
    0x00158593, // addi   a1, a1, 1
    0x0eb52c23, // sw     a1, 0x0F8(a0)
    0x00100073, // ebreak
  };

  load_prog(prog_get_block_unaligned);
  set_dbg(DM_DATA1, addr);

  uint8_t* cursor = (uint8_t*)dst;
  for (int i = 0; i < size; i++) {
    set_dbg(DM_COMMAND, cmd_runprog);
    while(busy());
    cursor[i] = get_dbg(DM_DATA0);
  }

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::set_block_aligned(uint32_t addr, void* src, int size) {
  CHECK(sanity());
  CHECK(halted());
  CHECK((addr & 3) == 0);
  CHECK((size & 3) == 0);

  static uint32_t prog_set_block_aligned[8] = {
    0xe0000537, // lui    a0, 0xE0000
    0x0f852583, // lw     a1, 0x0F8(a0)
    0x0f452503, // lw     a0, 0x0F4(a0)
    0x00a5a023, // sw     a0, a1
    0x00458593, // addi   a1, a1, 4
    0xe0000537, // lui    a0, 0xE0000
    0x0eb52c23, // sw     a1, 0xF8(a0)
    0x00100073, // ebreak
  };


  load_prog(prog_set_block_aligned);
  set_dbg(DM_DATA1, addr);

  uint32_t* cursor = (uint32_t*)src;
  for (int i = 0; i < size / 4; i++) {
    set_dbg(DM_DATA0, *cursor++);
    set_dbg(DM_COMMAND, cmd_runprog);
    while(busy());
  }

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::set_block_unaligned(uint32_t addr, void* src, int size) {
  CHECK(sanity());
  CHECK(halted());

  static uint32_t prog_set_block_aligned[8] = {
    0xe0000537, // lui    a0, 0xE0000
    0x0f852583, // lw     a1, 0x0F8(a0)
    0x0f452503, // lw     a0, 0x0F4(a0)
    0x00a58023, // sb     a0, 0x000(a1)
    0x00158593, // addi   a1, a1, 1
    0xe0000537, // lui    a0, 0xE0000
    0x0eb52c23, // sw     a1, 0x0F8(a0)
    0x00100073, // ebreak
  };

  load_prog(prog_set_block_aligned);
  set_dbg(DM_DATA1, addr);

  uint8_t* cursor = (uint8_t*)src;
  for (int i = 0; i < size; i++) {
    set_dbg(DM_DATA0, *cursor++);
    set_dbg(DM_COMMAND, cmd_runprog);
    while(busy());
  }

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::wipe_page(uint32_t dst_addr) {
  CHECK(sanity());
  CHECK(halted());
  unlock_flash();
  dst_addr |= 0x08000000;
  run_flash_command(dst_addr, BIT_CTLR_FTER, BIT_CTLR_FTER | BIT_CTLR_STRT);
  CHECK(sanity());
}

void SLDebugger::wipe_sector(uint32_t dst_addr) {
  CHECK(sanity());
  CHECK(halted());
  unlock_flash();
  dst_addr |= 0x08000000;
  run_flash_command(dst_addr, BIT_CTLR_PER, BIT_CTLR_PER | BIT_CTLR_STRT);
  CHECK(sanity());
}

void SLDebugger::wipe_chip() {
  CHECK(sanity());
  CHECK(halted());
  unlock_flash();
  uint32_t dst_addr = 0x08000000;
  run_flash_command(dst_addr, BIT_CTLR_MER, BIT_CTLR_MER | BIT_CTLR_STRT);
  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::write_flash2(uint32_t dst_addr, void* data, int size) {
  CHECK(sanity());
  CHECK(halted());

  if (size % 4) printf("SLDebugger::write_flash2() - Bad size %d\n", size);
  int size_dwords = size / 4;

  //printf("SLDebugger::write_flash 0x%08x 0x%08x %d\n", dst_addr, data, size);

  dst_addr |= 0x08000000;

  //halt();
  //unlock_flash();

  static const uint16_t prog_write_incremental[16] = {
    // Copy word and trigger BUFLOAD
    0x4180, // lw      s0,0(a1)
    0xc200, // sw      s0,0(a2)
    0xc914, // sw      a3,16(a0)

    // waitloop1: Busywait for copy to complete - this seems to be required now?
    0x4540, // lw      s0,12(a0)
    0x8805, // andi    s0,s0,1
    0xfc75, // bnez    s0, <waitloop1>

    // Advance dest pointer and trigger START if we ended a page
    0x0611, // addi    a2,a2,4
    0x7413, // andi    s0,a2,63
    0x03f6, //
    0xe419, // bnez    s0, <end>
    0xc918, // sw      a4,16(a0)

    // waitloop2: Busywait for page write to complete
    0x4540, // lw      s0,12(a0)
    0x8805, // andi    s0,s0,1
    0xfc75, // bnez    s0, <waitloop2>

    // Reset buffer, don't need busywait as it'll complete before we send the
    // next dword.
    0xc91c, // sw      a5,16(a0)

    // Update page address
    0xc950, // sw      a2,20(a0)
  };

  set_mem_u32_aligned(ADDR_FLASH_ADDR, dst_addr);
  set_mem_u32_aligned(ADDR_FLASH_CTLR, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  load_prog((uint32_t*)prog_write_incremental);
  set_gpr(10, 0x40022000); // flash base
  set_gpr(11, 0xE00000F4); // DATA0 @ 0xE00000F4
  set_gpr(12, dst_addr);
  set_gpr(13, BIT_CTLR_FTPG | BIT_CTLR_BUFLOAD);
  set_gpr(14, BIT_CTLR_FTPG | BIT_CTLR_STRT);
  set_gpr(15, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  bool first_word = true;
  int page_count = (size_dwords + 15) / 16;

  // Start feeding dwords to prog_write_incremental.

  int busy_loops = 0;

  for (int page = 0; page < page_count; page++) {
    //printf("!");
    for (int dword_idx = 0; dword_idx < 16; dword_idx++) {
      //printf(".");
      int offset = (page * SLDebugger::page_size) + (dword_idx * sizeof(uint32_t));
      uint32_t* src = (uint32_t*)((uint8_t*)data + offset);
      set_dbg(DM_DATA0, dword_idx < size_dwords ? *src : 0xDEADBEEF);
      if (first_word) {
        // There's a chip bug here - we can't set AUTOCMD before COMMAND or
        // things break all weird
        set_dbg(DM_COMMAND, cmd_runprog);
        set_dbg(DM_ABSTRACTAUTO, 0x00000001);
        first_word = false;
      }

      // We can write flash slightly faster if we only busy-wait at the end
      // of each page, but I am wary...
      while(busy()) busy_loops++;
    }
  }

  set_dbg(DM_ABSTRACTAUTO, 0x00000000);
  set_mem_u32_aligned(ADDR_FLASH_CTLR, 0);

  {
    auto statr = Reg_FLASH_STATR(get_mem_u32_aligned(ADDR_FLASH_STATR));
    statr.EOP = 1;
    set_mem_u32_aligned(ADDR_FLASH_STATR, statr);
  }

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::print_status() {
  CHECK(sanity());

  printf("--------------------------------------------------------------------------------\n");

  Reg_CPBR      (get_dbg(DM_CPBR)).dump();
  Reg_CFGR      (get_dbg(DM_CFGR)).dump();
  Reg_SHDWCFGR  (get_dbg(DM_SHDWCFGR)).dump();

  printf("----------\n");

  printf("REG_DATA0 = 0x%08x\n", get_dbg(DM_DATA0));
  printf("REG_DATA1 = 0x%08x\n", get_dbg(DM_DATA1));
  Reg_DMCONTROL (get_dbg(DM_DMCONTROL)).dump();
  Reg_DMSTATUS  (get_dbg(DM_DMSTATUS)).dump();
  Reg_HARTINFO  (get_dbg(DM_HARTINFO)).dump();
  Reg_ABSTRACTCS(get_dbg(DM_ABSTRACTCS)).dump();
  Reg_COMMAND   (get_dbg(DM_COMMAND)).dump();
  Reg_AUTOCMD   (get_dbg(DM_ABSTRACTAUTO)).dump();
  printf("PROG 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
    get_dbg(DM_PROGBUF0), get_dbg(DM_PROGBUF1), get_dbg(DM_PROGBUF2), get_dbg(DM_PROGBUF3),
    get_dbg(DM_PROGBUF4), get_dbg(DM_PROGBUF5), get_dbg(DM_PROGBUF6), get_dbg(DM_PROGBUF7));
  printf("REG_HALTSUM = 0x%08x\n", get_dbg(DM_HALTSUM0));

  printf("----------\n");

  if (halted()) {

    Reg_FLASH_STATR(get_mem_u32_aligned(ADDR_FLASH_STATR)).dump();
    Reg_FLASH_CTLR(get_mem_u32_aligned(ADDR_FLASH_CTLR)).dump();
    Reg_FLASH_OBR(get_mem_u32_aligned(ADDR_FLASH_OBR)).dump();
    printf("REG_FLASH_WPR = 0x%08x\n", get_mem_u32_aligned(ADDR_FLASH_WPR));

    Csr_DCSR(get_csr(CSR_DCSR)).dump();
    printf("DPC : 0x%08x\n", get_csr(CSR_DPC));
    printf("DS0 : 0x%08x\n", get_csr(CSR_DSCRATCH0));
    printf("DS1 : 0x%08x\n", get_csr(CSR_DSCRATCH1));

    printf("\n");
  }
  else {
    printf("<not halted>\n");
  }

  printf("--------------------------------------------------------------------------------\n");

  CHECK(sanity());
}

//------------------------------------------------------------------------------
// Dumps all ram

void SLDebugger::dump_ram() {
  CHECK(sanity());

  if (!halted()) {
    printf("SLDebugger::dump_ram() - Not halted\n");
  }

  //assert(halted);

  uint8_t temp[2048];

  get_block_aligned(0x20000000, temp, 2048);

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 16; x++) {
      printf("0x%08x ", ((uint32_t*)temp)[x + y * 16]);
    }
    printf("\n");
  }

  CHECK(sanity());
}

//------------------------------------------------------------------------------
// Dumps the first 1K of flash.

extern unsigned int blink_bin_len;

void SLDebugger::dump_flash() {
  CHECK(sanity());

  if (!halted()) {
    printf("SLDebugger::dump_flash() - Not halted\n");
    return;
  }

  Reg_FLASH_STATR(get_mem_u32_aligned(ADDR_FLASH_STATR)).dump();
  Reg_FLASH_CTLR(get_mem_u32_aligned(ADDR_FLASH_CTLR)).dump();
  Reg_FLASH_OBR(get_mem_u32_aligned(ADDR_FLASH_OBR)).dump();
  printf("flash_wpr 0x%08x\n", get_mem_u32_aligned(ADDR_FLASH_WPR));

  int lines = (blink_bin_len + 31) / 32;

  uint8_t temp[1024];
  get_block_aligned(0x00000000, temp, 1024);
  for (int y = 0; y < lines; y++) {
    for (int x = 0; x < 8; x++) {
      printf("0x%08x ", ((uint32_t*)temp)[x + y * 8]);
    }
    printf("\n");
  }

  CHECK(sanity());
}

//------------------------------------------------------------------------------

bool SLDebugger::test_mem() {
  CHECK(sanity());
  CHECK(halted());

  uint32_t base = 0x20000000;
  int size_dwords = 512;

  uint32_t addr;
  for (int i = 0; i < size_dwords; i++) {
    addr = base + (4 * i);
    set_mem_u32_aligned(addr, 0xBEEF0000 + i);
  }
  bool fail = false;
  for (int i = 0; i < size_dwords; i++) {
    addr = base + (4 * i);
    uint32_t data = get_mem_u32_aligned(addr);
    if (data != 0xBEEF0000 + i) {
      printf("Memory test fail at 0x%08x : expected 0x%08x, got 0x%08x\n", addr, 0xBEEF0000 + i, data);
      fail = true;
    }
  }

  Reg_ABSTRACTCS reg_abstractcs = get_dbg(DM_ABSTRACTCS);
  if (reg_abstractcs.CMDER) {
    printf("Memory test fail, CMDER=%d\n", reg_abstractcs.CMDER);
    fail = true;
  }

  //if (!fail) print("Memory test pass!\n");

  CHECK(sanity());
  return fail;
}

//------------------------------------------------------------------------------

void SLDebugger::run_flash_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2) {
  CHECK(sanity());
  //assert(halted);

  static const uint16_t prog_run_command[16] = {
    0xc94c, // sw      a1,20(a0)
    0xc910, // sw      a2,16(a0)
    0xc914, // sw      a3,16(a0)

    // waitloop:
    0x455c, // lw      a5,12(a0)
    0x8b85, // andi    a5,a5,1
    0xfff5, // bnez    a5, <waitloop>

    0x2823, // sw      zero,16(a0)
    0x0005, //
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
  };

  load_prog((uint32_t*)prog_run_command);
  set_gpr(10, 0x40022000);   // flash base
  set_gpr(11, addr);
  set_gpr(12, ctl1);
  set_gpr(13, ctl2);
  set_dbg(DM_COMMAND, cmd_runprog);
  while(busy());
  CHECK(sanity());
}

//------------------------------------------------------------------------------

void SLDebugger::load_prog(uint32_t* prog) {
  if (active_prog != prog) {
    for (int i = 0; i < 8; i++) {
      set_dbg(0x20 + i, prog[i]);
    }
    active_prog = prog;
  }
}

//------------------------------------------------------------------------------

void SLDebugger::save_regs() {
  for (int i = 0; i < reg_count; i++) {
    saved_regs[i] = get_gpr(i);
  }
  saved_pc = get_csr(CSR_DPC);
}

void SLDebugger::load_regs() {
  for (int i = 0; i < reg_count; i++) {
    set_gpr(i, saved_regs[i]);
  }
  set_csr(CSR_DPC, saved_pc);
}

//------------------------------------------------------------------------------

int SLDebugger::set_breakpoint(uint32_t addr) {
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] == addr) {
      printf("SLDebugger::set_breakpoint() - Breakpoint at 0x%08x already set\n");
      return -1;
    }
  }

  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] == 0) {
      breakpoints[i] = addr;
      return i;
    }
  }

  printf("SLDebugger::set_breakpoint() - No valid slots left\n");
  return -1;
}

//------------------------------------------------------------------------------

int SLDebugger::clear_breakpoint(uint32_t addr) {
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] == addr) {
      breakpoints[i] = 0;
      return i;
    }
  }

  return -1;
}

//------------------------------------------------------------------------------

void SLDebugger::dump_breakpoints() {
  printf("//----------\n");
  printf("Breakpoints:\n");
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i]) {
      printf("%03d: 0x%08x\n", i, breakpoints[i]);
    }
  }
  printf("//----------\n");
}

//------------------------------------------------------------------------------

void SLDebugger::patch_flash() {
  if (!halted()) {
    printf("Can't patch flash while running\n");
    return;
  }

  const int bitmap_size = (sizeof(flash_clean) / page_size) / 32;
  uint32_t patch_bitmap[bitmap_size] = {0};

  // Mark all pages with breakpoints in the bitmap.
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i]) {
      int page = breakpoints[i] / page_size;
      set_bit(patch_bitmap, page, 1);
    }
  }

  // Save a clean copy of all pages with breakpoints in them.
  for (int page = 0; page < page_count; page++) {
    if (get_bit(patch_bitmap, page)) {
      int page_addr = page * page_size;
      get_block_aligned(page_addr, flash_clean + page_addr, page_size);
      printf("clean 0x%08x\n", *(uint32_t*)(flash_clean + page_addr));
      memcpy(flash_dirty + page_addr, flash_clean + page_addr, page_size);
    }
  }

  // Replace all breakpoint addresses in flash_dirty with c.ebreak
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i]) {
      auto dst = (uint16_t*)(flash_dirty + breakpoints[i]);
      *dst = 0x9002;
    }
  }

  // Write all dirty pages to flash
  for (int page = 0; page < page_count; page++) {
    if (get_bit(patch_bitmap, page)) {
      printf("patching page %d\n", page);
      int page_addr = page * page_size;

      /*
      {
        uint32_t* base = (uint32_t*)(flash_clean + page_addr);

        for (int y = 0; y < 4; y++) {
          for (int x = 0; x < 4; x++) {
            printf("0x%08x ", base[x + y * 4]);
          }
          printf("\n");
        }
      }
      {
        uint32_t* base = (uint32_t*)(flash_dirty + page_addr);

        for (int y = 0; y < 4; y++) {
          for (int x = 0; x < 4; x++) {
            printf("0x%08x ", base[x + y * 4]);
          }
          printf("\n");
        }
      }
      */

      wipe_page(page_addr);
      write_flash2(page_addr, flash_dirty + page_addr, page_size);
    }
  }
}

//------------------------------------------------------------------------------

void SLDebugger::unpatch_flash() {
  if (!halted()) {
    printf("Can't unpatch flash while running\n");
    return;
  }

  const int bitmap_size = (sizeof(flash_clean) / page_size) / 32;
  uint32_t patch_bitmap[bitmap_size] = {0};

  // Mark all pages with breakpoints in the bitmap.
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i]) {
      int page = breakpoints[i] / page_size;
      set_bit(patch_bitmap, page, 1);
    }
  }

  // Replace all marked pages with a clean copy
  for (int page = 0; page < page_count; page++) {
    if (get_bit(patch_bitmap, page)) {
      printf("unpatching page %d\n", page);
      int page_addr = page * page_size;
      wipe_page(page_addr);
      write_flash2(page_addr, flash_clean + page_addr, page_size);
    }
  }
}

//------------------------------------------------------------------------------

void SLDebugger::step_over_breakpoint(uint32_t addr) {
  // Remove the breakpoint
  wipe_page(addr);
  write_flash2(addr, flash_clean + addr, page_size);

  // Step over the instruction there
  step();

  // Put the breakpoint back in
  wipe_page(addr);
  write_flash2(addr, flash_dirty + addr, page_size);
}

//------------------------------------------------------------------------------
