#include "WCH_Debug.h"

#include "build/singlewire.pio.h"

static const uint32_t cmd_runprog = 0x00040000;

volatile void busy_wait(int count) {
  volatile int c = count;
  while(c--);
}

//-----------------------------------------------------------------------------

void SLDebugger::init(int swd_pin) {
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
  sm_config_set_out_shift   (&c, /*shift_right*/ false, /*autopull*/ false, 32);
  sm_config_set_in_shift    (&c, /*shift_right*/ false, /*autopush*/ true, /*push_threshold*/ 32);
  sm_config_set_clkdiv      (&c, 10);

  pio_sm_init(pio0, sm, offset, &c);
  pio_sm_set_pins(pio0, 0, 0);
  pio_sm_set_enabled(pio0, sm, true);
}

//-----------------------------------------------------------------------------

void SLDebugger::reset() {
  active_prog = nullptr;
  pio0->ctrl = 0b000100010001; // Reset pio block

  // Grab pin and send a 2 msec low pulse to reset debug module (i think?)
  // If we use the sdk functions to do this we get jitter :/
  sio_hw->gpio_clr    = (1 << swd_pin);
  sio_hw->gpio_oe_set = (1 << swd_pin);
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  //busy_wait((10 * 120000000 / 1000) / 8);
  // busy_wait(84) = 5.80 us will     disable the debug interface = 46.40T
  // busy_wait(85) = 5.72 us will not disable the debug interface = 45.76T
  busy_wait(100); // ~8 usec
  sio_hw->gpio_oe_clr = (1 << swd_pin);
  //sio_hw->gpio_set  = (1 << swd_pin);
  //busy_wait(10); // wait ~
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // Enable debug output pin
  put(ADDR_SHDWCFGR, 0x5AA50400);
  put(ADDR_CFGR,     0x5AA50400);

  // Turn debug module on
  put(ADDR_DMCONTROL, 0x00000001);

  // Clear error status
  put(ADDR_ABSTRACTCS, 0x00000700);
}

//-----------------------------------------------------------------------------

void SLDebugger::halt() {
  put(ADDR_DMCONTROL, 0x80000001);
  while (!get_dmstatus().ALLHALTED);
  put(ADDR_DMCONTROL, 0x00000001);
}

void SLDebugger::unhalt() {
  put(ADDR_DMCONTROL, 0x40000001);
  while (get_dmstatus().ANYHALTED);
  put(ADDR_DMCONTROL, 0x00000001);
}

// Where did this come from?
void SLDebugger::stop_watchdogs() {
  put(ADDR_DATA0,     0x00000003);
  put(ADDR_COMMAND,   0x002307C0);
}

//-----------------------------------------------------------------------------

void SLDebugger::load_prog(uint32_t* prog) {
  if (active_prog != prog) {
    for (int i = 0; i < 8; i++) {
      put(0x20 + i, prog[i]);
    }
    active_prog = prog;
  }
}

//-----------------------------------------------------------------------------

void SLDebugger::get(uint8_t addr, void* out) const {
  addr = ((~addr) << 1) | 1;
  pio_sm_put_blocking(pio0, 0, addr);
  *(uint32_t*)out = pio_sm_get_blocking(pio0, 0);
}

void SLDebugger::put(uint8_t addr, uint32_t data) const {
  addr = ((~addr) << 1) | 0;
  data = ~data;
  pio_sm_put_blocking(pio0, 0, addr);
  pio_sm_put_blocking(pio0, 0, data);
}

//-----------------------------------------------------------------------------

void SLDebugger::get_mem32(uint32_t addr, void* data) {
  static uint32_t prog_get32[8] = {
    0x7b251073, // csrw 0x7b2, a0
    0x7b359073, // csrw 0x7b3, a1
    0xe0000537, // lui  a0, 0xe0000
    0x0f852583, // lw   a1, 248(a0)
    0x0005a583, // lw   a1, 0(a1)
    0x0eb52a23, // sw   a1, 244(a0)
    0x7b202573, // csrr a0, 0x7b2
    0x7b3025f3, // csrr a1, 0x7b3
  };

  load_prog(prog_get32);
  put(ADDR_DATA0,   0xDEADBEEF);
  put(ADDR_DATA1,   addr);
  put(ADDR_COMMAND, cmd_runprog);
  get(ADDR_DATA0,   data);
}

void SLDebugger::put_mem32(uint32_t addr, void* data) {
  static uint32_t prog_put32[8] = {
    0x7b251073, // csrw 0x7b2,a0
    0x7b359073, // csrw 0x7b3,a1
    0xe0000537, // lui  a0,0xe0000
    0x0f852583, // lw   a1,248(a0)
    0x0f452503, // lw   a0,244(a0)
    0x00a5a023, // sw   a0,0(a1)
    0x7b202573, // csrr a0,0x7b2
    0x7b3025f3, // csrr a1,0x7b3
  };

  load_prog(prog_put32);
  put(ADDR_DATA0,   *(uint32_t*)data);
  put(ADDR_DATA1,   addr);
  put(ADDR_COMMAND, cmd_runprog);
}

//-----------------------------------------------------------------------------

const Reg_DMSTATUS SLDebugger::get_dmstatus() const {
  Reg_DMSTATUS r;
  get(ADDR_DMSTATUS, &r);
  return r;
}

const Reg_ABSTRACTCS SLDebugger::get_abstatus() const {
  Reg_ABSTRACTCS r;
  get(ADDR_ABSTRACTCS, &r);
  return r;
}

//-----------------------------------------------------------------------------
// Seems to work

uint32_t SLDebugger::get_gpr(int index) const {
  Reg_COMMAND cmd = {0};
  cmd.REGNO      = 0x1000 | index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  put(ADDR_DATA0, 0xDEADBEEF);
  put(ADDR_COMMAND, cmd.raw);
  while(get_abstatus().BUSY);

  uint32_t result = 0;
  get(ADDR_DATA0, &result);
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

  put(ADDR_DATA0,   gpr);
  put(ADDR_COMMAND, cmd.raw);
  while(get_abstatus().BUSY);
}

//-----------------------------------------------------------------------------

uint32_t SLDebugger::get_csr(int index) const {
  Reg_COMMAND cmd = {0};

  cmd.REGNO      = index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  //put(ADDR_DATA0, 0x00000000);
  put(ADDR_COMMAND, cmd.raw);
  while(get_abstatus().BUSY);

  uint32_t result = 0;
  get(ADDR_DATA0, &result);
  return result;
}

//------------------------------------------------------------------------------

void SLDebugger::save_regs() {
  for (int i = 0; i < reg_count; i++) {
    regfile[i] = get_gpr(i);
  }
  reg_saved = true;
}

void SLDebugger::load_regs() {
  for (int i = 0; i < reg_count; i++) {
    put_gpr(i, regfile[i]);
  }
  reg_saved = true;
}

//-----------------------------------------------------------------------------

#if 0

void SingleLineReadMEMDirectBlock(uint32_t addr, uint16_t *psize,
                                     uint32_t *pdbuf) {
  uint32_t const *p32;
  uint8_t i;
  uint16_t len = *psize - 1;

  DWordSend(DEG_Abst_DATA1, addr);
  if (len == 0) {
    p32 = CH32V003_R_32bit;
    for (i = 0; i < 8; i++) DWordSend(DEG_Prog_BUF0 + (i << 1), *p32++);
    DWordSend(DEG_Abst_CMD, (1 << 18)); /* execute pro buf */
    // SingleLineCheckAbstSTA();
    DWordRecv(DEG_Abst_DATA0, pdbuf);
  } else {
    DWordSend(DEG_Abst_CMDAUTO, 0x00000001);
    p32 = CH32V003_R_W_blk;
    for (i = 0; i < 8; i++) DWordSend(DEG_Prog_BUF0 + (i << 1), *p32++);
    DWordSend(DEG_Abst_CMD, (1 << 18)); /* execute pro buf */
    BlockRecv(DEG_Abst_DATA0, pdbuf, len);
    *psize = len;
    DWordSend(DEG_Abst_CMDAUTO, 0x00000000);
    // SingleLineCheckAbstSTA();
    DWordRecv(DEG_Abst_DATA0, pdbuf + len);
    *psize = len + 1;
  }
}

void SLDebugger::get_mem_block(uint32_t base, int size_bytes, void* out) {
  int size_dwords = size_bytes >> 2;
  if (size_dwords == 0) return;

  // Block read prog from  1line_base
  static uint32_t prog_block32[8] = {
    0xe0000537, 0x0f450513, 0xf693414c, 0x8563ffc5,
    0x411000d5, 0xa019c290, 0xc1104290, 0xc14c0591
  };

  load_prog(prog_block32);

  put(ADDR_DATA1, base);
  put(ADDR_AUTOCMD, 0x00000001);  // every time we read DATA0 run the command again
  put(ADDR_COMMAND, cmd_runprog);

  uint32_t* cursor = (uint32_t*)out;

  for (int i = 0; i < size_dwords - 1; i++) {
    /*
    DIO_OUT();
    Pulse(4);
    Put8(addr);
    for (int i = 0; i < ndwords; i++) {
      datbuf[i] = DataBitRecv32();
      // Restore bus high
      DIO_OUT();
      SWDIO_Out_1();
      if (i < ndwords-1) Pulse(42); // why 42 for inter-block start bit?
    }
    DIO_IN();
    delay(162);
    */
  }

  put(ADDR_AUTOCMD, 0x00000000);
  get(ADDR_DATA0,   cursor++);
}
#endif


//------------------------------------------------------------------------------

bool test_mem(SLDebugger& sl) {
  uint32_t base = 0x20000000;
  int size_dwords = 512;

  uint32_t addr;
  uint32_t data;
  for (int i = 0; i < size_dwords; i++) {
    addr = base + (4 * i);
    data = 0xBEEF0000 + i;
    sl.put_mem32(addr, &data);
  }
  bool fail = false;
  for (int i = 0; i < size_dwords; i++) {
    addr = base + (4 * i);
    data = 0xDEADBEEF;
    sl.get_mem32(addr, &data);
    if (data != 0xBEEF0000 + i) {
      printf("Memory test fail at 0x%08x : expected 0x%08x, got 0x%08x\n", addr, 0xBEEF0000 + i, data);
      fail = true;
    }
  }

  Reg_ABSTRACTCS reg_abstractcs;
  sl.get(ADDR_ABSTRACTCS, &reg_abstractcs);
  if (reg_abstractcs.CMDER) {
    printf("Memory test fail, CMDER=%d\n", reg_abstractcs.CMDER);
    fail = true;
  }

  //if (!fail) printf("Memory test pass!\n");
  return fail;
}


#if 0
    uint32_t part0, part1, part2, part3, part4;
    sl.get_mem32(0x1FFFF7E0, &part0);
    sl.get_mem32(0x1FFFF7E8, &part1);
    sl.get_mem32(0x1FFFF7EC, &part2);
    sl.get_mem32(0x1FFFF7F0, &part3);
    sl.get_mem32(0x1FFFF7C4, &part4);

    printf("part0 0x%08x\n", part0);
    printf("part1 0x%08x\n", part1);
    printf("part2 0x%08x\n", part2);
    printf("part3 0x%08x\n", part3);
    printf("part4 0x%08x\n", part4);
    printf("\n");

#endif

#if 0
    Reg_CPBR       reg_cpbr;
    Reg_CFGR       reg_cfgr;
    Reg_SHDWCFGR   reg_shdwcfgr;

    sl.halt();

    sl.get(ADDR_CPBR,       &reg_cpbr);
    sl.get(ADDR_CFGR,       &reg_cfgr);
    sl.get(ADDR_SHDWCFGR,   &reg_shdwcfgr);

    sl.unhalt();

    printf("\033c");
    printf("rep    %d\n", rep++);
    printf("reg_cpbr\n");       reg_cpbr.dump();       printf("\n");
    printf("reg_cfgr\n");       reg_cfgr.dump();       printf("\n");
    printf("reg_shdwcfgr\n");   reg_shdwcfgr.dump();   printf("\n");
#endif

#if 0
    // Test memory
    bool fail = test_mem(sl);

    //printf("\033c");
    printf("rep    %d\n", rep++);
    printf("fail : %d\n", fail);
#endif

#if 0
    // Check error status register1
    Reg_ABSTRACTCS reg_abstractcs;
    sl.get(ADDR_ABSTRACTCS, &reg_abstractcs);
    sl.unhalt();
    printf("\n");
    printf("reg_abstractcs\n"); reg_abstractcs.dump(); printf("\n");
#endif

#if 0
    Reg_DMCTRL     reg_dmctrl;
    Reg_DMSTATUS   reg_dmstatus;
    Reg_HARTINFO   reg_hartinfo;
    Reg_ABSTRACTCS reg_abstractcs;
    Reg_COMMAND    reg_command;
    Reg_AUTOCMD    reg_autocmd;
    Reg_HALTSUM0   reg_haltsum0;

    sl.halt();

    sl.get(ADDR_DMCONTROL,  &reg_dmctrl);
    sl.get(ADDR_DMSTATUS,   &reg_dmstatus);
    sl.get(ADDR_HARTINFO,   &reg_hartinfo);
    sl.get(ADDR_ABSTRACTCS, &reg_abstractcs);
    sl.get(ADDR_COMMAND,    &reg_command);
    sl.get(ADDR_AUTOCMD,    &reg_autocmd);
    sl.get(ADDR_HALTSUM0,   &reg_haltsum0);

    sl.unhalt();

    printf("\033c");
    printf("rep    %d\n", rep++);
    printf("reg_dmctrl\n");     reg_dmctrl.dump();     printf("\n");
    printf("reg_dmstatus\n");   reg_dmstatus.dump();   printf("\n");
    printf("reg_command\n");    reg_command.dump();    printf("\n");
    printf("reg_autocmd\n");    reg_autocmd.dump();    printf("\n");
    printf("reg_abstractcs\n"); reg_abstractcs.dump(); printf("\n");
    printf("reg_hartinfo\n");   reg_hartinfo.dump();   printf("\n");
    printf("reg_haltsum0\n");   reg_haltsum0.dump();   printf("\n");
#endif
