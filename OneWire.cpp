#include "OneWire.h"
#include "utils.h"
#include "WCH_Regs.h"
#include "build/singlewire.pio.h"

  /*
  auto acs = Reg_ABSTRACTCS(sl->get_dbg(DM_ABSTRACTCS));
  if (acs.BUSY) {
    printf("BUSY   = %d\n", acs.BUSY);
  }
  if (acs.CMDER) {
    printf("CMDERR = %d\n", acs.CMDER);
    sl->clear_err();
  }
  */


void busy_wait(int count) {
  volatile int c = count;
  while(c) c = c - 1;
}

//------------------------------------------------------------------------------

void OneWire::init_pio(int swd_pin) {
  this->swd_pin = swd_pin;

  //gpio_set_drive_strength(swd_pin, GPIO_DRIVE_STRENGTH_12MA);
  //gpio_set_slew_rate     (swd_pin, GPIO_SLEW_RATE_FAST);
  gpio_set_drive_strength(swd_pin, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate     (swd_pin, GPIO_SLEW_RATE_SLOW);
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

void OneWire::reset_dbg() {
  CHECK(swd_pin != -1);

  active_prog = nullptr;
  
  // Reset pio block
  pio0->ctrl = 0b000100010001; 

  // Grab pin and send an 8 usec low pulse to reset debug module
  // If we use the sdk functions to do this we get jitter :/
  sio_hw->gpio_clr    = (1 << swd_pin);
  sio_hw->gpio_oe_set = (1 << swd_pin);
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  busy_wait(100); // ~8 usec
  sio_hw->gpio_oe_clr = (1 << swd_pin);
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // Enable debug output pin
  set_dbg(WCH_DM_SHDWCFGR, 0x5AA50400);
  set_dbg(WCH_DM_CFGR,     0x5AA50400);

  // Reset debug module
  set_dbg(DM_DMCONTROL, 0x00000000);
  set_dbg(DM_DMCONTROL, 0x00000001);
}

//------------------------------------------------------------------------------

const char* addr_to_regname(uint8_t addr) {
  switch(addr) {
    case WCH_DM_CPBR:     return "WCH_DM_CPBR";
    case WCH_DM_CFGR:     return "WCH_DM_CFGR";
    case WCH_DM_SHDWCFGR: return "WCH_DM_SHDWCFGR";
    case WCH_DM_PART:     return "WCH_DM_PART";

    case DM_DATA0:        return "DM_DATA0";
    case DM_DATA1:        return "DM_DATA1";
    case DM_DMCONTROL:    return "DM_DMCONTROL";
    case DM_DMSTATUS:     return "DM_DMSTATUS";
    case DM_HARTINFO:     return "DM_HARTINFO";
    case DM_ABSTRACTCS:   return "DM_ABSTRACTCS";
    case DM_COMMAND:      return "DM_COMMAND";
    case DM_ABSTRACTAUTO: return "DM_ABSTRACTAUTO";
    case DM_PROGBUF0:     return "DM_PROGBUF0";
    case DM_PROGBUF1:     return "DM_PROGBUF1";
    case DM_PROGBUF2:     return "DM_PROGBUF2";
    case DM_PROGBUF3:     return "DM_PROGBUF3";
    case DM_PROGBUF4:     return "DM_PROGBUF4";
    case DM_PROGBUF5:     return "DM_PROGBUF5";
    case DM_PROGBUF6:     return "DM_PROGBUF6";
    case DM_PROGBUF7:     return "DM_PROGBUF7";
    case DM_HALTSUM0:     return "DM_HALTSUM0";
    default:              return "???";
  }
  
}

uint32_t OneWire::get_dbg(uint8_t addr) {
  cmd_count++;
  pio_sm_put_blocking(pio0, 0, ((~addr) << 1) | 1);
  auto data = pio_sm_get_blocking(pio0, 0);
  printf("get_dbg %15s 0x%08x\n", addr_to_regname(addr), data);
  return data;
}

//------------------------------------------------------------------------------

void OneWire::set_dbg(uint8_t addr, uint32_t data) {
  cmd_count++;
  printf("set_dbg %15s 0x%08x\n", addr_to_regname(addr), data);
  pio_sm_put_blocking(pio0, 0, ((~addr) << 1) | 0);
  pio_sm_put_blocking(pio0, 0, ~data);
}

//------------------------------------------------------------------------------

void OneWire::halt() {
  set_dbg(DM_DMCONTROL, 0x80000001);
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED);
  set_dbg(DM_DMCONTROL, 0x00000001);
}

//------------------------------------------------------------------------------

void OneWire::resume() {
  set_dbg(DM_DMCONTROL, 0x40000001);
  while(!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLRESUMEACK);
  set_dbg(DM_DMCONTROL, 0x00000001);
}

//------------------------------------------------------------------------------

void OneWire::step() {
  Csr_DCSR dcsr = get_csr(CSR_DCSR);
  dcsr.STEP = 1;
  set_csr(CSR_DCSR, dcsr);

  resume();
  while(!halted());

  dcsr.STEP = 0;
  set_csr(CSR_DCSR, dcsr);
}

//------------------------------------------------------------------------------

bool OneWire::halted() {
  return Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED;
}

//------------------------------------------------------------------------------

void OneWire::reset_cpu_and_halt() {
  // Halt and leave halt request set
  set_dbg(DM_DMCONTROL, 0x80000001);
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED);

  // Set reset request
  set_dbg(DM_DMCONTROL, 0x80000003);
  while(!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHAVERESET);

  // Clear reset request and hold halt request
  set_dbg(DM_DMCONTROL, 0x80000001);
  // this busywait seems to be required or we hang
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED); 

  // Clear HAVERESET
  set_dbg(DM_DMCONTROL, 0x90000001);
  while(Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHAVERESET);

  // Clear halt request
  set_dbg(DM_DMCONTROL, 0x00000001);
}

//------------------------------------------------------------------------------

void OneWire::load_prog(uint32_t* prog) {
  if (active_prog != prog) {
    for (int i = 0; i < 8; i++) {
      set_dbg(0x20 + i, prog[i]);
    }
    active_prog = prog;
  }
}

//------------------------------------------------------------------------------

uint32_t OneWire::get_data0() {
  return get_dbg(DM_DATA0);
}

uint32_t OneWire::get_data1() {
  return get_dbg(DM_DATA1);
}

void OneWire::set_data0(uint32_t d) {
  set_dbg(DM_DATA0, d);
}

void OneWire::set_data1(uint32_t d) {
  set_dbg(DM_DATA1, d);
}

//------------------------------------------------------------------------------

uint32_t OneWire::get_gpr(int index) {
  CHECK(halted && sanity());

  Reg_COMMAND cmd;
  cmd.REGNO      = 0x1000 | index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(DM_COMMAND, cmd);
  // pretty sure we don't need this
  //while(Reg_ABSTRACTCS(get_dbg(DM_ABSTRACTCS)).BUSY);
  auto result = get_dbg(DM_DATA0);

  CHECK(halted && sanity());
  return result;
}

//------------------------------------------------------------------------------

void OneWire::set_gpr(int index, uint32_t gpr) {
  CHECK(halted && sanity());

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
    // pretty sure we don't need this
    //while(Reg_ABSTRACTCS(get_dbg(DM_ABSTRACTCS)).BUSY);
  }

  CHECK(halted && sanity());
}

//------------------------------------------------------------------------------

uint32_t OneWire::get_csr(int index) {
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
  // pretty sure we don't need this
  //while(Reg_ABSTRACTCS(get_dbg(DM_ABSTRACTCS)).BUSY);
  auto result = get_dbg(DM_DATA0);
  CHECK(sanity());
  return result;
}

//------------------------------------------------------------------------------

void OneWire::set_csr(int index, uint32_t data) {
  CHECK(sanity());

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
  // pretty sure we don't need this
  //while(Reg_ABSTRACTCS(get_dbg(DM_ABSTRACTCS)).BUSY);

  CHECK(sanity());
}

//------------------------------------------------------------------------------

void OneWire::run_prog() {
  const uint32_t cmd_runprog = 0x00040000;
  set_dbg(DM_COMMAND, cmd_runprog);
  // we definitely need this busywait
  while(Reg_ABSTRACTCS(get_dbg(DM_ABSTRACTCS)).BUSY);
}

//------------------------------------------------------------------------------

void OneWire::clear_err() {
  set_dbg(DM_ABSTRACTCS, 0x00030000);
}

//------------------------------------------------------------------------------

bool OneWire::sanity() {
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

void OneWire::status() {
  printf("----------------------------------------\n");
  printf("OneWire::status()\n");
  printf("SWD pin:  %d\n", swd_pin);
  printf("Halted:   %d\n", Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED);

  printf("----------------------------------------\n");
  Reg_CPBR      (get_dbg(WCH_DM_CPBR)).dump();
  Reg_CFGR      (get_dbg(WCH_DM_CFGR)).dump();
  Reg_SHDWCFGR  (get_dbg(WCH_DM_SHDWCFGR)).dump();

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
}

//------------------------------------------------------------------------------


#if 0
void OneWire::reset_cpu_and_halt() {
  CHECK(attached);
  CHECK(sanity());

  // Halt and leave halt request set
  set_dbg(DM_DMCONTROL, 0x80000001);
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED);
  halted = true;

  // Set reset request
  set_dbg(DM_DMCONTROL, 0x80000003);
  while(!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHAVERESET);

  // Clear reset request and hold halt request
  set_dbg(DM_DMCONTROL, 0x80000001);
  // this busywait seems to be required or we hang
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED); 

  // Clear HAVERESET
  set_dbg(DM_DMCONTROL, 0x90000001);
  while(Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHAVERESET);

  // Clear halt request
  set_dbg(DM_DMCONTROL, 0x00000001);

  save_regs();

  // Resetting the CPU resets DCSR, update it again.
  patch_dcsr();

  CHECK(sanity());
}

//------------------------------------------------------------------------------

bool OneWire::test_mem() {
  CHECK(halted && sanity());

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

  CHECK(halted && sanity());
  return fail;
}

#endif



//------------------------------------------------------------------------------

uint32_t OneWire::get_mem_u32_aligned(uint32_t addr) {
  if (addr & 3) {
    printf("OneWire::get_mem_u32_aligned() - Bad address 0x%08x\n", addr);
    return 0;
  }

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
  set_data1(addr);
  run_prog();

  auto result = get_data0();

  return result;
}

//------------------------------------------------------------------------------

uint32_t OneWire::get_mem_u32_unaligned(uint32_t addr) {
  auto offset  = addr & 3;
  
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  uint32_t buf[2];
  buf[0] = get_mem_u32_aligned(addr_lo);
  buf[1] = get_mem_u32_aligned(addr_hi);

  return *((uint32_t*)((uint8_t*)buf + offset));
}

//------------------------------------------------------------------------------

uint16_t OneWire::get_mem_u16(uint32_t addr) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 1) & ~3;

  uint32_t buf[2];

  if (addr_lo == addr_hi) {
    buf[0] = get_mem_u32_aligned(addr_lo);
    return *((uint16_t*)((uint8_t*)buf + offset));
  }
  else {
    buf[0] = get_mem_u32_aligned(addr_lo);
    buf[1] = get_mem_u32_aligned(addr_hi);
    return *((uint16_t*)((uint8_t*)buf + offset));
  }
}

//------------------------------------------------------------------------------

uint8_t OneWire::get_mem_u8(uint32_t addr) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;

  uint32_t buf[2];
  buf[0] = get_mem_u32_aligned(addr_lo);
  return *((uint8_t*)buf + offset);
}

//------------------------------------------------------------------------------

void OneWire::set_mem_u32_aligned(uint32_t addr, uint32_t data) {
  if (addr & 3) printf("OneWire::set_mem_u32_aligned - Bad alignment 0x%08x\n", addr);

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

  set_data0(data);
  set_data1(addr);
  run_prog();
}

//------------------------------------------------------------------------------

void OneWire::set_mem_u32_unaligned(uint32_t addr, uint32_t data) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  uint32_t buf[2];
  buf[0] = get_mem_u32_aligned(addr_lo);
  buf[1] = get_mem_u32_aligned(addr_hi);

  *((uint32_t*)((uint8_t*)buf + offset)) = data;

  set_mem_u32_aligned(addr_lo, buf[0]);
  set_mem_u32_aligned(addr_hi, buf[1]);
}

//------------------------------------------------------------------------------

void OneWire::set_mem_u16(uint32_t addr, uint16_t data) {
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

void OneWire::set_mem_u8(uint32_t addr, uint8_t data) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;

  uint32_t buf[1];

  buf[0] = get_mem_u32_aligned(addr_lo);
  *((uint8_t*)buf + offset) = data;
  set_mem_u32_aligned(addr_lo, buf[0]);

  return;
}

//------------------------------------------------------------------------------
// FIXME we should handle invalid address ranges somehow...

void OneWire::get_block_aligned(uint32_t addr, void* dst, int size) {
  CHECK(halted, "OneWire::get_block_aligned() not halted");
  CHECK((addr & 3) == 0, "OneWire::get_block_aligned() bad address");
  CHECK((size & 3) == 0, "OneWire::get_block_aligned() bad size");

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
  set_data1(addr);

  uint32_t* cursor = (uint32_t*)dst;
  for (int i = 0; i < size / 4; i++) {
    run_prog();
    cursor[i] = get_data0();
  }
}

//------------------------------------------------------------------------------

void OneWire::get_block_unaligned(uint32_t addr, void* dst, int size) {
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
  set_data1(addr);

  uint8_t* cursor = (uint8_t*)dst;
  for (int i = 0; i < size; i++) {
    run_prog();
    cursor[i] = get_data0();
  }
}

//------------------------------------------------------------------------------

void OneWire::set_block_aligned(uint32_t addr, void* src, int size) {
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
  set_data1(addr);

  uint32_t* cursor = (uint32_t*)src;
  for (int i = 0; i < size / 4; i++) {
    set_data0(*cursor++);
    run_prog();
  }
}

//------------------------------------------------------------------------------

void OneWire::set_block_unaligned(uint32_t addr, void* src, int size) {
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
  set_data1(addr);

  uint8_t* cursor = (uint8_t*)src;
  for (int i = 0; i < size; i++) {
    set_data0(*cursor++);
    run_prog();
  }
}

//------------------------------------------------------------------------------
