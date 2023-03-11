#include "WCH_Debug.h"

#include "build/singlewire.pio.h"

uint32_t prog_get32[8] = {
  0x7B251073, 0x7B359073, 0xE0000537, 0x0F852583,
  0x2A23418C, 0x25730EB5, 0x25F37B20, 0x90027B30,
};

// Block read prog from  1line_base
uint32_t prog_block32[8] = {
  0xe0000537, 0x0f450513, 0xf693414c, 0x8563ffc5,
  0x411000d5, 0xa019c290, 0xc1104290, 0xc14c0591
};

static const uint32_t cmd_runprog = 0x00040000;

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
  sm_config_set_sideset     (&c, 1, /*optional*/ false, /*pindirs*/ false);
  sm_config_set_out_pins    (&c, swd_pin, 1);
  sm_config_set_in_pins     (&c, swd_pin);
  sm_config_set_set_pins    (&c, swd_pin, 1);
  sm_config_set_sideset_pins(&c, swd_pin);
  sm_config_set_out_shift   (&c, /*shift_right*/ false, /*autopull*/ false, 32);
  sm_config_set_in_shift    (&c, /*shift_right*/ false, /*autopush*/ true, /*push_threshold*/ 32);
  sm_config_set_clkdiv      (&c, 10);

  pio_sm_init(pio0, sm, offset, &c);
  pio_sm_set_enabled(pio0, sm, true);
}

//-----------------------------------------------------------------------------

volatile void busy_wait(int count) {
  volatile int c = count;
  while(c--);
}

void SLDebugger::reset() {
  // Grab pin and send a 2 msec low pulse to reset debug module (i think?)
  // If we use the sdk functions to do this we get jitter :/
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  sio_hw->gpio_clr    = (1 << swd_pin);
  sio_hw->gpio_oe_set = (1 << swd_pin);
  //busy_wait((10 * 120000000 / 1000) / 8);
  // busy_wait(84) = 5.80 us will     disable the debug interface = 46.40T
  // busy_wait(85) = 5.72 us will not disable the debug interface = 45.76T
  busy_wait(100); // ~8 usec
  sio_hw->gpio_set  = (1 << swd_pin);
  busy_wait(100);
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  active_prog = nullptr;
  // Reset pio block
  pio_sm_set_enabled(pio0, 0, false);
  pio0->ctrl = 0b000100010001;
  pio_sm_set_enabled(pio0, 0, true);

  put(ADDR_SHDWCFGR, 0x5AA50400);
  put(ADDR_CFGR,     0x5AA50400);
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

void SLDebugger::load_prog(uint32_t* prog) {
  if (active_prog != prog) {
    for (int i = 0; i < 8; i++) {
      put(0x20 + i, prog[i]);
    }
    active_prog = prog;
  }
}

void SLDebugger::read_bus32(uint32_t addr, void* out) {
  uint32_t result = 0xDEADBEEF;
  load_prog(prog_get32);
  put(ADDR_DATA1,   addr);
  put(ADDR_COMMAND, 0x00040000);
  get(ADDR_DATA0,   out);
}

void SLDebugger::halt() {
  put(ADDR_DMCONTROL, 0x80000001);
  //while (!get_dmstatus().ANYHALTED);
  put(ADDR_DMCONTROL, 0x00000001);
}

void SLDebugger::unhalt() {
  put(ADDR_DMCONTROL, 0x40000001);
  //while (get_dmstatus().ANYHALTED);
  put(ADDR_DMCONTROL, 0x00000001);
}

// Where did this come from?
void SLDebugger::stop_watchdogs() {
  put(ADDR_DATA0,     0x00000003);
  put(ADDR_COMMAND,   0x002307C0);
}

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
  uint32_t result = 0;
  Reg_COMMAND cmd = {0};

  cmd.REGNO      = 0x1000 | index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  put(ADDR_DATA0, 0x00000000);
  put(ADDR_COMMAND, cmd.raw);
  while(get_abstatus().BUSY);
  get(ADDR_DATA0, &result);
  return result;
}

//-----------------------------------------------------------------------------

uint32_t SLDebugger::get_csr(int index) const {
  uint32_t result = 0;
  Reg_COMMAND cmd = {0};

  cmd.REGNO      = index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  put(ADDR_DATA0, 0x00000000);
  put(ADDR_COMMAND, cmd.raw);
  while(get_abstatus().BUSY);
  get(ADDR_DATA0, &result);
  return result;
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
#endif

void SLDebugger::get_mem_block(uint32_t base, int size_bytes, void* out) {
  int size_dwords = size_bytes >> 2;
  if (size_dwords == 0) return;

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