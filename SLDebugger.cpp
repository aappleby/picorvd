#include "SLDebugger.h"
#include "WCH_Regs.h"
#include <assert.h>

#include "build/singlewire.pio.h"

static const uint32_t cmd_runprog = 0x00040000;

volatile void busy_wait(int count) {
  volatile int c = count;
  while(c--);
}

//-----------------------------------------------------------------------------

void SLDebugger::init(int swd_pin, putter cb_put) {
  this->cb_put = cb_put;
  this->swd_pin = swd_pin;

  gpio_set_drive_strength(swd_pin, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate(swd_pin, GPIO_SLEW_RATE_SLOW);
  gpio_set_function(swd_pin, GPIO_FUNC_PIO0);

  int sm = 0;
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

  pio_sm_init(pio0, sm, offset, &c);
  pio_sm_set_pins(pio0, sm, 0);
  pio_sm_set_enabled(pio0, sm, true);
}

//-----------------------------------------------------------------------------

void SLDebugger::reset() {
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
  set_dbg(ADDR_SHDWCFGR, 0x5AA50400);
  set_dbg(ADDR_CFGR,     0x5AA50400);

  // Reset debug module
  set_dbg(ADDR_DMCONTROL, 0x00000000);
  set_dbg(ADDR_DMCONTROL, 0x00000001);

  this->halted = Reg_DMSTATUS(get_dbg(ADDR_DMSTATUS)).ALLHALTED;
  this->locked = Reg_FLASH_CTLR(get_mem(ADDR_FLASH_CTLR)).LOCK;

#if 0
  // Turn debug module on and clear ACKHAVERESET
  put_dbg(ADDR_DMCONTROL, 0x10000001);

  // Debug module is on, halt the CPU and set up for debugging.
  halt();

  // Stop timers and watchdogs
  // 0x7C0 is a CSR that controls watchdogs and timers in debug mode
  put_dbg(ADDR_DATA0,     0b00110011);
  put_dbg(ADDR_COMMAND,   0x002307C0);

  // Unlock flash
  unlock_flash();

  // Clear error status
  clear_err();

#endif
}

//-----------------------------------------------------------------------------

void SLDebugger::halt() {
  if (halted) return;
  halted = true;

  // Halt CPU
  set_dbg(ADDR_DMCONTROL, 0x80000001);

  while (!Reg_DMSTATUS(get_dbg(ADDR_DMSTATUS)).ALLHALTED);
  set_dbg(ADDR_DMCONTROL, 0x00000001);
}

//-----------------------------------------------------------------------------

void SLDebugger::unhalt() {
  if (!halted) return;
  halted = false;

  // Clear reset and resume CPU
  set_dbg(ADDR_DMCONTROL, 0x50000001);
  while (Reg_DMSTATUS(get_dbg(ADDR_DMSTATUS)).ANYHALTED);

  // Clear ackresume
  set_dbg(ADDR_DMCONTROL, 0x00000001);
}

//------------------------------------------------------------------------------

void SLDebugger::lock_flash() {
  assert(halted);
  if (locked) return;
  locked = true;

  halt();
  auto ctlr = Reg_FLASH_CTLR(get_mem(ADDR_FLASH_CTLR));
  ctlr.LOCK = true;
  set_mem(ADDR_FLASH_CTLR, ctlr);
}

void SLDebugger::unlock_flash() {
  assert(halted);
  if (!locked) return;
  locked = false;

  set_mem(ADDR_FLASH_KEYR, 0x45670123);
  set_mem(ADDR_FLASH_KEYR, 0xCDEF89AB);
  set_mem(ADDR_FLASH_MKEYR, 0x45670123);
  set_mem(ADDR_FLASH_MKEYR, 0xCDEF89AB);
}

//------------------------------------------------------------------------------

void SLDebugger::reset_cpu() {
  print_to(cb_put, "SLDebugger::reset_cpu()\n");

  // Set halt request and wait for halt
  halt();

  // Set reset request and wait for reset
  print_to(cb_put, "SLDebugger::reset_cpu 2\n");
  set_dbg(ADDR_DMCONTROL, 0x80000003);
  while(!Reg_DMSTATUS(get_dbg(ADDR_DMSTATUS)).ALLHAVERESET);

  // Clear reset request and hold halt
  print_to(cb_put, "SLDebugger::reset_cpu 3\n");
  set_dbg(ADDR_DMCONTROL, 0x80000001);

  // Clear ALLHAVERESET and hold halt request
  print_to(cb_put, "SLDebugger::reset_cpu 4\n");
  set_dbg(ADDR_DMCONTROL, 0x90000001);
  while(Reg_DMSTATUS(get_dbg(ADDR_DMSTATUS)).ALLHAVERESET);

  // Clear halt request
  print_to(cb_put, "SLDebugger::reset_cpu 5\n");
  set_dbg(ADDR_DMCONTROL, 0x00000001);
}

//------------------------------------------------------------------------------

void SLDebugger::step() {
  assert(halted);

  Csr_DCSR dcsr = get_csr(CSR_DCSR);
  dcsr.STEP = 1;
  set_csr(CSR_DCSR, dcsr);

  set_dbg(ADDR_DMCONTROL, 0x40000001);
  while (Reg_DMSTATUS(get_dbg(ADDR_DMSTATUS)).ANYHALTED);
  set_dbg(ADDR_DMCONTROL, 0x80000001);
  while (!Reg_DMSTATUS(get_dbg(ADDR_DMSTATUS)).ALLHALTED);
}

//-----------------------------------------------------------------------------

void SLDebugger::clear_err() {
  set_dbg(ADDR_ABSTRACTCS, 0xFFFFFFFF);
}

//-----------------------------------------------------------------------------

uint32_t SLDebugger::get_dbg(uint8_t addr) {
  addr = ((~addr) << 1) | 1;
  pio_sm_put_blocking(pio0, 0, addr);
  return pio_sm_get_blocking(pio0, 0);
}

//-----------------------------------------------------------------------------

void SLDebugger::set_dbg(uint8_t addr, uint32_t data) {
  addr = ((~addr) << 1) | 0;
  data = ~data;
  pio_sm_put_blocking(pio0, 0, addr);
  pio_sm_put_blocking(pio0, 0, data);
}

//-----------------------------------------------------------------------------

uint32_t SLDebugger::get_gpr(int index) {
  assert(halted);

  Reg_COMMAND cmd;
  cmd.REGNO      = 0x1000 | index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(ADDR_COMMAND, cmd);
  return get_dbg(ADDR_DATA0);
}

//-----------------------------------------------------------------------------

void SLDebugger::set_gpr(int index, uint32_t gpr) {
  assert(halted);

  Reg_COMMAND cmd;
  cmd.REGNO      = 0x1000 | index;
  cmd.WRITE      = 1;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(ADDR_DATA0,   gpr);
  set_dbg(ADDR_COMMAND, cmd);
}

//-----------------------------------------------------------------------------

uint32_t SLDebugger::get_csr(int index) {
  assert(halted);

  Reg_COMMAND cmd;
  cmd.REGNO      = index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(ADDR_COMMAND, cmd);
  return get_dbg(ADDR_DATA0);
}

//-----------------------------------------------------------------------------

void SLDebugger::set_csr(int index, uint32_t data) {
  assert(halted);

  Reg_COMMAND cmd;
  cmd.REGNO      = index;
  cmd.WRITE      = 1;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(ADDR_DATA0, data);
  set_dbg(ADDR_COMMAND, cmd);
}

//-----------------------------------------------------------------------------

uint32_t SLDebugger::get_mem(uint32_t addr) {
  assert(halted);

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

  set_dbg(ADDR_DATA0,   0xDEADBEEF);
  set_dbg(ADDR_DATA1,   addr);
  set_dbg(ADDR_COMMAND, cmd_runprog);
  auto result = get_dbg(ADDR_DATA0);

  return result;
}

//-----------------------------------------------------------------------------

void SLDebugger::set_mem(uint32_t addr, uint32_t data) {
  assert(halted);

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

  set_dbg(ADDR_DATA0,   data);
  set_dbg(ADDR_DATA1,   addr);
  set_dbg(ADDR_COMMAND, cmd_runprog);
}

//-----------------------------------------------------------------------------

static uint32_t prog_get_block[8] = {
  0xe0000537, // lui    a0, 0xE0000
  0x0f852583, // lw     a1, 0x0F8(a0) // data0 = *data1;
  0x0005a583, // lw     a1, 0x000(a1)
  0x0eb52a23, // sw     a1, 0x0F4(a0)
  0x0f852583, // lw     a1, 0x0F8(a0) // data1 += 4
  0x00458593, // addi   a1, a1, 4
  0x0eb52c23, // sw     a1, 0x0F8(a0)
  0x00100073, // ebreak
};

void SLDebugger::get_block(uint32_t addr, void* data, int size_dwords) {
  stream_get_start(addr);
  uint32_t* cursor = (uint32_t*)data;
  for (int i = 0; i < size_dwords; i++) {
    *cursor++ = stream_get_u32();
  }
  stream_get_end();
}

void SLDebugger::stream_get_start(uint32_t addr) {
  assert(halted);
  load_prog(prog_get_block);
  set_dbg(ADDR_DATA1,   addr);
  set_dbg(ADDR_AUTOCMD, 0x00000001);
  set_dbg(ADDR_COMMAND, cmd_runprog);
}

uint32_t SLDebugger::stream_get_u32() {
  assert(halted);
  return get_dbg(ADDR_DATA0);
}

void SLDebugger::stream_get_end() {
  assert(halted);
  set_dbg(ADDR_AUTOCMD, 0x00000000);
}


//-----------------------------------------------------------------------------

static uint32_t prog_put_block[8] = {
  0xe0000537, // lui    a0, 0xE0000
  0x0f852583, // lw     a1, 0x0F8(a0)
  0x0f452503, // lw     a0, 0x0F4(a0)
  0x00a5a023, // sw     a0, a1
  0x00458593, // addi   a1, a1, 4
  0xe0000537, // lui    a0, 0xE0000
  0x0eb52c23, // sw     a1, 0xF8(a0)
  0x00100073, // ebreak
};

void SLDebugger::set_block(uint32_t addr, void* data, int size_dwords) {

  uint32_t* cursor = (uint32_t*)data;

  load_prog(prog_put_block);
  set_dbg(ADDR_DATA0, *cursor++);
  set_dbg(ADDR_DATA1, addr);
  set_dbg(ADDR_AUTOCMD, 0x00000001);
  set_dbg(ADDR_COMMAND, cmd_runprog);
  for (int i = 0; i < size_dwords - 1; i++) {
    set_dbg(ADDR_DATA0, *cursor++);
  }

  set_dbg(ADDR_AUTOCMD, 0x00000000);
}

void SLDebugger::stream_set_start(uint32_t addr) {
  assert(halted);
  load_prog(prog_put_block);
  set_dbg(ADDR_DATA1, addr);
  first_put = true;
}

void SLDebugger::stream_set_u32(uint32_t data) {
  assert(halted);
  set_dbg(ADDR_DATA0, data);
  if (first_put) {
    set_dbg(ADDR_AUTOCMD, 0x00000001);
    set_dbg(ADDR_COMMAND, cmd_runprog);
    first_put = false;
  }
}

void SLDebugger::stream_set_end() {
  assert(halted);
  set_dbg(ADDR_AUTOCMD, 0x00000000);
}

//------------------------------------------------------------------------------

void SLDebugger::wipe_page(uint32_t addr) {
  assert(halted);
  assert(!locked);
  run_command(0, BIT_CTLR_FTER, BIT_CTLR_FTER | BIT_CTLR_STRT);
}

void SLDebugger::wipe_sector(uint32_t addr) {
  assert(halted);
  assert(!locked);
  run_command(0, BIT_CTLR_PER, BIT_CTLR_PER | BIT_CTLR_STRT);
}

void SLDebugger::wipe_chip() {
  assert(halted);
  assert(!locked);
  run_command(0, BIT_CTLR_MER, BIT_CTLR_MER | BIT_CTLR_STRT);
}

//------------------------------------------------------------------------------

void SLDebugger::write_flash(uint32_t dst_addr, uint32_t* data, int size_dwords) {
  assert(halted);
  assert(!locked);

  print_to(cb_put, "SLDebugger::write_flash 0x%08x 0x%08x %d\n", dst_addr, data, size_dwords);

  unlock_flash();

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

  set_mem(ADDR_FLASH_ADDR, dst_addr);
  set_mem(ADDR_FLASH_CTLR, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  load_prog((uint32_t*)prog_write_incremental);
  set_gpr(10, 0x40022000); // flash base
  set_gpr(11, 0xE00000F4); // DATA0 @ 0xE00000F4
  set_gpr(12, dst_addr);
  set_gpr(13, BIT_CTLR_FTPG | BIT_CTLR_BUFLOAD);
  set_gpr(14, BIT_CTLR_FTPG | BIT_CTLR_STRT);
  set_gpr(15, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  bool first_word = true;
  int page_count = (size_dwords + 15) / 16;

  // Start feeding dwords to prog_write_incremental. The busy-wait loops above
  // will complete before we finish sending the next word, except at the end
  // of a page when it takes ~3 msec to finish the write.

  for (int page = 0; page < page_count; page++) {
    for (int i = 0; i < 16; i++) {
      int index = page * 16 + i;
      set_dbg(ADDR_DATA0, index < size_dwords ? data[page * 16 + i] : 0xDEADBEEF);
      if (first_word) {
        // There's a chip bug here - we can't set AUTOCMD before COMMAND or
        // things break all weird
        set_dbg(ADDR_COMMAND, cmd_runprog);
        set_dbg(ADDR_AUTOCMD, 0x00000001);
        first_word = false;
      }
    }

    // Sychronize with prog_write_incremental so we don't send another dword
    // until the page is done writing.
    while(Reg_ABSTRACTCS(get_dbg(ADDR_ABSTRACTCS)).BUSY);
  }

  set_mem(ADDR_FLASH_CTLR, 0);
}

//-----------------------------------------------------------------------------

void SLDebugger::print_status() {

  Reg_CPBR      (get_dbg(ADDR_CPBR)).dump(cb_put);
  Reg_CFGR      (get_dbg(ADDR_CFGR)).dump(cb_put);
  Reg_SHDWCFGR  (get_dbg(ADDR_SHDWCFGR)).dump(cb_put);
  Reg_DMCONTROL (get_dbg(ADDR_DMCONTROL)).dump(cb_put);
  Reg_DMSTATUS  (get_dbg(ADDR_DMSTATUS)).dump(cb_put);
  Reg_ABSTRACTCS(get_dbg(ADDR_ABSTRACTCS)).dump(cb_put);

  if (halted) {
    Csr_DCSR(get_csr(CSR_DCSR)).dump(cb_put);
    print_to(cb_put, "dpc:0x%08x  ds0:0x%08x  ds1:0x%08x\n", get_csr(CSR_DPC), get_csr(CSR_DS0), get_csr(CSR_DS1));
    print_to(cb_put, "\n");
  }
  else {
    print_to(cb_put, "<not halted\n");
  }
}

//------------------------------------------------------------------------------
// Dumps all ram

void SLDebugger::dump_ram() {
  assert(halted);

  uint32_t temp[512];

  get_block(0x20000000, temp, 512);

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 16; x++) {
      print_to(cb_put, "0x%08x ", temp[x + y * 16]);
    }
    print_to(cb_put, "\n");
  }
}

//------------------------------------------------------------------------------
// Dumps the first 1K of flash.

void SLDebugger::dump_flash() {
  assert(halted);

  Reg_FLASH_STATR(get_mem(ADDR_FLASH_STATR)).dump(cb_put);
  Reg_FLASH_CTLR(get_mem(ADDR_FLASH_CTLR)).dump(cb_put);
  Reg_FLASH_OBR(get_mem(ADDR_FLASH_OBR)).dump(cb_put);
  print_to(cb_put, "flash_wpr 0x%08x\n", get_mem(ADDR_FLASH_WPR));

  uint32_t temp[256];
  get_block(0x08000000, temp, 256);
  //for (int y = 0; y < 32; y++) {
  for (int y = 0; y < 24; y++) {
    for (int x = 0; x < 8; x++) {
      print_to(cb_put, "0x%08x ", temp[x + y * 8]);
    }
    print_to(cb_put, "\n");
  }
}

//-----------------------------------------------------------------------------

bool SLDebugger::test_mem() {
  assert(halted);

  uint32_t base = 0x20000000;
  int size_dwords = 512;

  uint32_t addr;
  for (int i = 0; i < size_dwords; i++) {
    addr = base + (4 * i);
    set_mem(addr, 0xBEEF0000 + i);
  }
  bool fail = false;
  for (int i = 0; i < size_dwords; i++) {
    addr = base + (4 * i);
    uint32_t data = get_mem(addr);
    if (data != 0xBEEF0000 + i) {
      print_to(cb_put, "Memory test fail at 0x%08x : expected 0x%08x, got 0x%08x\n", addr, 0xBEEF0000 + i, data);
      fail = true;
    }
  }

  Reg_ABSTRACTCS reg_abstractcs = get_dbg(ADDR_ABSTRACTCS);
  if (reg_abstractcs.CMDER) {
    print_to(cb_put, "Memory test fail, CMDER=%d\n", reg_abstractcs.CMDER);
    fail = true;
  }

  //if (!fail) print("Memory test pass!\n");
  return fail;
}

//-----------------------------------------------------------------------------

void SLDebugger::run_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2) {
  assert(halted);

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
  set_dbg(ADDR_COMMAND, cmd_runprog);
}

//-----------------------------------------------------------------------------

void SLDebugger::load_prog(uint32_t* prog) {
  if (active_prog != prog) {
    for (int i = 0; i < 8; i++) {
      set_dbg(0x20 + i, prog[i]);
    }
    active_prog = prog;
  }
}

//------------------------------------------------------------------------------
