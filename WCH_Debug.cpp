#include "WCH_Debug.h"

#include "build/singlewire.pio.h"

static const uint32_t cmd_runprog = 0x00040000;

volatile void busy_wait(int count) {
  volatile int c = count;
  while(c--);
}

//-----------------------------------------------------------------------------

void SLDebugger::init(int swd_pin, cb_printf print) {
  this->print = print;
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
  put_dbg(ADDR_SHDWCFGR, 0x5AA50400);
  put_dbg(ADDR_CFGR,     0x5AA50400);

  // Turn debug module on and clear ACKHAVERESET
  put_dbg(ADDR_DMCONTROL, 0x10000001);

  // Debug module is on, halt the CPU and set up for debugging.
  halt();
}

//-----------------------------------------------------------------------------

void SLDebugger::halt() {
  // Halt CPU
  put_dbg(ADDR_DMCONTROL, 0x80000001);
  while (!get_dmstatus().ALLHALTED);
  put_dbg(ADDR_DMCONTROL, 0x00000001);

  // Stop timers and watchdogs
  // 0x7C0 is a CSR that controls watchdogs and timers in debug mode
  put_dbg(ADDR_DATA0,     0b00110011);
  put_dbg(ADDR_COMMAND,   0x002307C0);

  // Unlock flash
  put_mem32(ADDR_FLASH_KEYR, 0x45670123);
  put_mem32(ADDR_FLASH_KEYR, 0xCDEF89AB);
  put_mem32(ADDR_FLASH_MKEYR, 0x45670123);
  put_mem32(ADDR_FLASH_MKEYR, 0xCDEF89AB);

  // Clear error status
  put_dbg(ADDR_ABSTRACTCS, 0x00000700);

  // Save registers
  for (int i = 0; i < reg_count; i++) regfile[i] = get_gpr(i);
}

//-----------------------------------------------------------------------------

void SLDebugger::unhalt(bool reset) {
  if (reset) {
    // Reset CPU
    put_dbg(ADDR_DMCONTROL, 0x00000003);
    while(!get_dmstatus().ALLHAVERESET); 
  }
  else {
    // Reload registers if we're not resetting the CPU
    for (int i = 0; i < reg_count; i++) put_gpr(i, regfile[i]);
  }

  // Clear reset and resume CPU
  put_dbg(ADDR_DMCONTROL, 0x50000001);
  while (get_dmstatus().ANYHALTED);

  // Clear ackresume
  put_dbg(ADDR_DMCONTROL, 0x00000001);
}

//-----------------------------------------------------------------------------

uint32_t SLDebugger::get_dbg(uint8_t addr) {
  addr = ((~addr) << 1) | 1;
  pio_sm_put_blocking(pio0, 0, addr);
  return pio_sm_get_blocking(pio0, 0);
}

void SLDebugger::put_dbg(uint8_t addr, uint32_t data) {
  addr = ((~addr) << 1) | 0;
  data = ~data;
  pio_sm_put_blocking(pio0, 0, addr);
  pio_sm_put_blocking(pio0, 0, data);
}

void SLDebugger::get_dbgp(uint8_t addr, void* out) {
  *(uint32_t*)out = get_dbg(addr);
}

//-----------------------------------------------------------------------------

void SLDebugger::load_prog(uint32_t* prog) {
  if (active_prog != prog) {
    for (int i = 0; i < 8; i++) {
      put_dbg(0x20 + i, prog[i]);
    }
    active_prog = prog;
  }
}

//-----------------------------------------------------------------------------

static uint32_t prog_get32[8] = {
  0xe0000537, // lui  a0, 0xE0000
  0x0f852583, // lw   a1, 0x0F8(a0)
  0x0005a583, // lw   a1, 0x000(a1)
  0x0eb52a23, // sw   a1, 0x0F4(a0)
  0x00100073, // ebreak
  0x00100073, // ebreak
  0x00100073, // ebreak
  0x00100073, // ebreak
};

static uint32_t prog_put32[8] = {
  0xe0000537, // lui  a0, 0xE0000
  0x0f852583, // lw   a1, 0x0F8(a0)
  0x0f452503, // lw   a0, 0x0F4(a0)
  0x00a5a023, // sw   a0, 0x000(a1)
  0x00100073, // ebreak
  0x00100073, // ebreak
  0x00100073, // ebreak
  0x00100073, // ebreak
};

static uint32_t prog_put16[8] = {
  0xe0000537, // lui  a0, 0xE0000
  0x0f852583, // lw   a1, 0x0F8(a0)
  0x0f452503, // lw   a0, 0x0F4(a0)
  0x00a59023, // sh   a0, 0x000(a1)
  0x00100073, // ebreak
  0x00100073, // ebreak
  0x00100073, // ebreak
  0x00100073, // ebreak
};

uint32_t SLDebugger::get_mem32(uint32_t addr) {
  uint32_t data;
  get_mem32p(addr, &data);
  return data;
}

void SLDebugger::put_mem32(uint32_t addr, uint32_t data) {
  load_prog(prog_put32);
  put_dbg(ADDR_DATA0,   data);
  put_dbg(ADDR_DATA1,   addr);
  put_dbg(ADDR_COMMAND, cmd_runprog);
}

void SLDebugger::get_mem32p(uint32_t addr, void* data) {
  load_prog(prog_get32);
  put_dbg(ADDR_DATA0,   0xDEADBEEF);
  put_dbg(ADDR_DATA1,   addr);
  put_dbg(ADDR_COMMAND, cmd_runprog);
  get_dbgp(ADDR_DATA0,  data);
}

void SLDebugger::put_mem32p(uint32_t addr, void* data) {
  put_mem32(addr, *(uint32_t*)data);
}

void SLDebugger::put_mem16(uint32_t addr, uint16_t data) {
  load_prog(prog_put16);
  put_dbg(ADDR_DATA0,   data);
  put_dbg(ADDR_DATA1,   addr);
  put_dbg(ADDR_COMMAND, cmd_runprog);
}

//-----------------------------------------------------------------------------

void SLDebugger::get_block32(uint32_t addr, void* data, int size_dwords) {
  static uint32_t prog_getblock32[8] = {
    0xe0000537, // lui    a0, 0xE0000
    0x0f852583, // lw     a1, 0x0F8(a0) // data0 = *data1;
    0x0005a583, // lw     a1, 0x000(a1)
    0x0eb52a23, // sw     a1, 0x0F4(a0)
    0x0f852583, // lw     a1, 0x0F8(a0) // data1 += 4
    0x00458593, // addi   a1, a1, 4
    0x0eb52c23, // sw     a1, 0x0F8(a0)
    0x00100073, // ebreak
  };

  uint32_t* cursor = (uint32_t*)data;

  load_prog(prog_getblock32);
  put_dbg(ADDR_DATA1,   addr);
  put_dbg(ADDR_AUTOCMD, 0x00000001);
  put_dbg(ADDR_COMMAND, cmd_runprog);
  for (int i = 0; i < size_dwords; i++) {
    get_dbgp(ADDR_DATA0, cursor++);
  }

  put_dbg(ADDR_AUTOCMD, 0x00000000);
}

//-----------------------------------------------------------------------------

void SLDebugger::put_block32(uint32_t addr, void* data, int size_dwords) {
  static uint32_t prog_putblock32[8] = {
    0xe0000537, // lui    a0, 0xE0000
    0x0f852583, // lw     a1, 0x0F8(a0)
    0x0f452503, // lw     a0, 0x0F4(a0)
    0x00a5a023, // sw     a0, a1
    0x00458593, // addi   a1, a1, 4
    0xe0000537, // lui    a0, 0xE0000
    0x0eb52c23, // sw     a1, 0xF8(a0)
    0x00100073, // ebreak
  };

  uint32_t* cursor = (uint32_t*)data;
  uint32_t a0 = get_gpr(10);
  uint32_t a1 = get_gpr(11);

  load_prog(prog_putblock32);
  put_dbg(ADDR_DATA0, *cursor++);
  put_dbg(ADDR_DATA1, addr);
  put_dbg(ADDR_AUTOCMD, 0x00000001);
  put_dbg(ADDR_COMMAND, cmd_runprog);
  for (int i = 0; i < size_dwords - 1; i++) {
    put_dbg(ADDR_DATA0, *cursor++);
  }

  put_dbg(ADDR_AUTOCMD, 0x00000000);
  put_gpr(10, a0);
  put_gpr(11, a1);
}

//-----------------------------------------------------------------------------
// Seems to work

uint32_t SLDebugger::get_gpr(int index) {
  Reg_COMMAND cmd = {0};
  cmd.REGNO      = 0x1000 | index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  uint32_t result = 0;
  put_dbg(ADDR_COMMAND, cmd.raw);
  get_dbgp(ADDR_DATA0, &result);
  return result;
}

void SLDebugger::put_gpr(int index, uint32_t gpr) {
  Reg_COMMAND cmd = {0};
  cmd.REGNO      = 0x1000 | index;
  cmd.WRITE      = 1;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  put_dbg(ADDR_DATA0,   gpr);
  put_dbg(ADDR_COMMAND, cmd.raw);
}

//-----------------------------------------------------------------------------

void SLDebugger::get_csrp(int index, void* data) {
  Reg_COMMAND cmd = {0};
  cmd.REGNO      = index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  put_dbg(ADDR_COMMAND, cmd.raw);
  get_dbgp(ADDR_DATA0, data);
}

void SLDebugger::put_csrp(int index, void* data) {
  Reg_COMMAND cmd = {0};
  cmd.REGNO      = index;
  cmd.WRITE      = 1;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  put_dbg(ADDR_DATA0, *(uint32_t*)data);
  put_dbg(ADDR_COMMAND, cmd.raw);
}

uint32_t SLDebugger::get_csr(int index) {
  Reg_COMMAND cmd = {0};
  cmd.REGNO      = index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  put_dbg(ADDR_COMMAND, cmd.raw);
  return get_dbg(ADDR_DATA0);
}

void SLDebugger::put_csr(int index, uint32_t data) {
  Reg_COMMAND cmd = {0};
  cmd.REGNO      = index;
  cmd.WRITE      = 1;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  put_dbg(ADDR_DATA0, data);
  put_dbg(ADDR_COMMAND, cmd.raw);
}

//------------------------------------------------------------------------------

void SLDebugger::step() {
  Reg_DCSR dcsr = get_dcsr();
  dcsr.STEP = 1;
  put_gpr(0x7B0, dcsr.raw);
  put_dbg(ADDR_DMCONTROL, 0x40000001);
  while (get_dmstatus().ANYHALTED);
  put_dbg(ADDR_DMCONTROL, 0x80000001);
  while (!get_dmstatus().ALLHALTED);
}

//-----------------------------------------------------------------------------

bool SLDebugger::test_mem() {
  uint32_t base = 0x20000000;
  int size_dwords = 512;

  uint32_t addr;
  for (int i = 0; i < size_dwords; i++) {
    addr = base + (4 * i);
    put_mem32(addr, 0xBEEF0000 + i);
  }
  bool fail = false;
  for (int i = 0; i < size_dwords; i++) {
    addr = base + (4 * i);
    uint32_t data = 0xDEADBEEF;
    get_mem32p(addr, &data);
    if (data != 0xBEEF0000 + i) {
      print("Memory test fail at 0x%08x : expected 0x%08x, got 0x%08x\n", addr, 0xBEEF0000 + i, data);
      fail = true;
    }
  }

  Reg_ABSTRACTCS reg_abstractcs;
  get_dbgp(ADDR_ABSTRACTCS, &reg_abstractcs);
  if (reg_abstractcs.CMDER) {
    print("Memory test fail, CMDER=%d\n", reg_abstractcs.CMDER);
    fail = true;
  }

  //if (!fail) print("Memory test pass!\n");
  return fail;
}

//-----------------------------------------------------------------------------

void SLDebugger::print_status() {
  Reg_CPBR       reg_cpbr;
  Reg_CFGR       reg_cfgr;
  Reg_SHDWCFGR   reg_shdwcfgr;
  Reg_DMCONTROL  reg_dmctrl;
  Reg_DMSTATUS   reg_dmstatus;
  Reg_ABSTRACTCS reg_abstractcs;

  Reg_DCSR dcsr;
  uint32_t dpc  = 0xDEADBEEF;
  uint32_t ds0  = 0xDEADBEEF;
  uint32_t ds1  = 0xDEADBEEF;

  //----------

  reg_cpbr.raw       = get_dbg(ADDR_CPBR);
  reg_cfgr.raw       = get_dbg(ADDR_CFGR);
  reg_shdwcfgr.raw   = get_dbg(ADDR_SHDWCFGR);
  reg_dmctrl.raw     = get_dbg(ADDR_DMCONTROL);
  reg_dmstatus.raw   = get_dbg(ADDR_DMSTATUS);
  reg_abstractcs.raw = get_dbg(ADDR_ABSTRACTCS);

  get_csrp(0x7B0, &dcsr);
  get_csrp(0x7B1, &dpc);
  get_csrp(0x7B2, &ds0);
  get_csrp(0x7B3, &ds1);

  //----------

  print("reg_dmcontrol\n");  reg_dmctrl.dump(print);     print("\n");
  print("reg_dmstatus\n");   reg_dmstatus.dump(print);   print("\n");
  print("reg_abstractcs\n"); reg_abstractcs.dump(print); print("\n");
  print("reg_dcsr\n");       dcsr.dump(print);           print("\n");
  print("dpc  0x%08x\n", dpc);
  print("ds0  0x%08x\n", ds0);
  print("ds1  0x%08x\n", ds1);
  print("\n");
}

//------------------------------------------------------------------------------

uint16_t prog_run_command[16] = {
  0xc94c, // sw      a1,20(a0)
  0xc910, // sw      a2,16(a0)
  0xc914, // sw      a3,16(a0)
  0x455c, // lw      a5,12(a0)
  0x8b85, // andi    a5,a5,1
  0xfff5, // bnez    a5,17c <_Z14do_flash_thingRV9Reg_FLASHPvmm+0x6>
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

void SLDebugger::run_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2) {
  load_prog((uint32_t*)prog_run_command);
  put_gpr(10, 0x40022000);   // flash base
  put_gpr(11, addr);            // not using addr
  put_gpr(12, ctl1);
  put_gpr(13, ctl2);
  put_dbg(ADDR_COMMAND, cmd_runprog);
}

//------------------------------------------------------------------------------

void SLDebugger::wipe_64(uint32_t addr) {
  run_command(0, BIT_CTLR_FTER, BIT_CTLR_FTER | BIT_CTLR_STRT);
}

void SLDebugger::wipe_1024(uint32_t addr) {
  run_command(0, BIT_CTLR_PER, BIT_CTLR_PER | BIT_CTLR_STRT);
}

void SLDebugger::wipe_chip() {
  run_command(0, BIT_CTLR_MER, BIT_CTLR_MER | BIT_CTLR_STRT);
}

//------------------------------------------------------------------------------

uint16_t prog_write_incremental[16] = {
  // copy word and trigger BUFLOAD
  0x4180, // lw      s0,0(a1)
  0xc200, // sw      s0,0(a2)
  0xc914, // sw      a3,16(a0)

  // wait for ack - this seems to be required now....?
  0x4540, // lw      s0,12(a0)
  0x8805, // andi    s0,s0,1
  0xfc75, // bnez    s0,15a <waitloop1>

  // advance dest pointer and trigger START if we ended a page
  0x0611, // addi    a2,a2,4
  0x7413, // andi    s0,a2,63
  0x03f6, //
  0xe419, // bnez    s0,174 <end>
  0xc918, // sw      a4,16(a0)

  // wait for write to complete
  0x4540, // lw      s0,12(a0)
  0x8805, // andi    s0,s0,1
  0xfc75, // bnez    s0,16a <waitloop2>

  // reset buffer, don't need to wait for BUSY as it'll be reset before we can
  // upload another dword
  0xc91c, // sw      a5,16(a0)

  // update address
  0xc950, // sw      a2,20(a0)
};

//------------------------------------------------------------------------------

void SLDebugger::write_flash(uint32_t dst_addr, uint32_t* data, int size_dwords) {
  put_mem32(ADDR_FLASH_ADDR, dst_addr);
  put_mem32(ADDR_FLASH_CTLR, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  load_prog((uint32_t*)prog_write_incremental);
  put_gpr(10, 0x40022000); // flash base
  put_gpr(11, 0xE00000F4); // DATA0 @ 0xE00000F4
  put_gpr(12, dst_addr);
  put_gpr(13, BIT_CTLR_FTPG | BIT_CTLR_BUFLOAD);
  put_gpr(14, BIT_CTLR_FTPG | BIT_CTLR_STRT);
  put_gpr(15, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);
 
  bool first_word = true;
  int page_count = (size_dwords + 15) / 16;
  int counter = 0;

  for (int page = 0; page < page_count; page++) {
    for (int i = 0; i < 16; i++) {
      int index = page * 16 + i;
      put_dbg(ADDR_DATA0, index < size_dwords ? data[page * 16 + i] : 0xDEADBEEF);
      if (first_word) {
        // There's a chip bug here - we can't set AUTOCMD before COMMAND or
        // things break all weird
        put_dbg(ADDR_COMMAND, cmd_runprog);
        put_dbg(ADDR_AUTOCMD, 0x00000001);
        first_word = false;
      }
    }
    // Wait for write to complete after each page.
    while(get_abstatus().BUSY);
  }

  put_mem32(ADDR_FLASH_CTLR, 0);
}

//------------------------------------------------------------------------------

void SLDebugger::dump_ram() {
  uint32_t temp[512];

  get_block32(0x20000000, temp, 512);

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 16; x++) {
      print("0x%08x ", temp[x + y * 16]);
    }
    print("\n");
  }
}

//------------------------------------------------------------------------------

void SLDebugger::dump_flash() {
  uint32_t temp[256];

  Reg_FLASH_STATR s;
  Reg_FLASH_CTLR c;
  Reg_FLASH_OBR o;
  uint32_t flash_wpr = 0;

  //get_mem32p(ADDR_FLASH_WPR, &flash_wpr);
  flash_wpr = get_mem32(ADDR_FLASH_WPR);

  s.raw = get_mem32(ADDR_FLASH_STATR);
  //get_mem32p(ADDR_FLASH_STATR, &s);


  get_mem32p(ADDR_FLASH_CTLR, &c);
  get_mem32p(ADDR_FLASH_OBR, &o);
  get_block32(0x08000000, temp, 256);

  print("flash_wpr 0x%08x\n", flash_wpr);
  print("Reg_FLASH_STATR\n");
  s.dump(print);
  print("Reg_FLASH_CTLR\n");
  c.dump(print);
  print("Reg_FLASH_OBR\n");
  o.dump(print);

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      print("0x%08x ", temp[x + y * 16]);
    }
    print("\n");
  }
}

//------------------------------------------------------------------------------
