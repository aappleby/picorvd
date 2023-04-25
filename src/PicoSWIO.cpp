#include "PicoSWIO.h"
#include "bin/singlewire.pio.h"
#include "debug_defines.h"

#include "utils.h"

#define DUMP_COMMANDS

__attribute__((noinline)) void busy_wait(int count) {
  volatile int c = count;
  while (c) c = c - 1;
}

// WCH-specific debug interface config registers
static const int WCH_DM_CPBR     = 0x7C;
static const int WCH_DM_CFGR     = 0x7D;
static const int WCH_DM_SHDWCFGR = 0x7E;
static const int WCH_DM_PART     = 0x7F; // not in doc but appears to be part info

//------------------------------------------------------------------------------

void PicoSWIO::reset(int pin) {
  CHECK(pin != -1);
  this->pin = pin;

  // Configure GPIO
  gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate     (pin, GPIO_SLEW_RATE_SLOW);
  gpio_set_function      (pin, GPIO_FUNC_PIO0);

  // Reset PIO module
  pio0->ctrl = 0b000100010001;
  pio_sm_set_enabled(pio0, pio_sm, false);

  // Upload PIO program
  pio_clear_instruction_memory(pio0);
  uint pio_offset = pio_add_program(pio0, &singlewire_program);

  // Configure PIO module
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap        (&c, pio_offset + singlewire_wrap_target, pio_offset + singlewire_wrap);
  sm_config_set_sideset     (&c, 1, /*optional*/ false, /*pindirs*/ true);
  sm_config_set_out_pins    (&c, pin, 1);
  sm_config_set_in_pins     (&c, pin);
  sm_config_set_set_pins    (&c, pin, 1);
  sm_config_set_sideset_pins(&c, pin);
  sm_config_set_out_shift   (&c, /*shift_right*/ false, /*autopull*/ false, /*pull_threshold*/ 32);
  sm_config_set_in_shift    (&c, /*shift_right*/ false, /*autopush*/ true,  /*push_threshold*/ 32);

  // 125 mhz / 12 = 96 nanoseconds per tick, close enough to 100 ns.
  sm_config_set_clkdiv      (&c, 12);

  pio_sm_init       (pio0, pio_sm, pio_offset, &c);
  pio_sm_set_pins   (pio0, pio_sm, 0);
  pio_sm_set_enabled(pio0, pio_sm, true);

  // Grab pin and send an 8 usec low pulse to reset debug module
  // If we use the sdk functions to do this we get jitter :/
  sio_hw->gpio_clr    = (1 << pin);
  sio_hw->gpio_oe_set = (1 << pin);
  iobank0_hw->io[pin].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  busy_wait(100); // ~8 usec
  sio_hw->gpio_oe_clr = (1 << pin);
  iobank0_hw->io[pin].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // Enable debug output pin on target
  put(WCH_DM_SHDWCFGR, 0x5AA50400);
  put(WCH_DM_CFGR,     0x5AA50400);

  // Reset debug module on target
  put(DM_DMCONTROL, 0x00000000);
  put(DM_DMCONTROL, 0x00000001);
}

//------------------------------------------------------------------------------

uint32_t PicoSWIO::get(uint32_t addr) {
  cmd_count++;
  pio_sm_put_blocking(pio0, 0, ((~addr) << 1) | 1);
  auto data = pio_sm_get_blocking(pio0, 0);
#ifdef DUMP_COMMANDS
  LOG("get_dbg %15s 0x%08x\n", addr_to_regname(addr), data);
#endif
  return data;
}

//------------------------------------------------------------------------------

void PicoSWIO::put(uint32_t addr, uint32_t data) {
  cmd_count++;
#ifdef DUMP_COMMANDS
  LOG("set_dbg %15s 0x%08x\n", addr_to_regname(addr), data);
#endif
  pio_sm_put_blocking(pio0, 0, ((~addr) << 1) | 0);
  pio_sm_put_blocking(pio0, 0, ~data);
}

//------------------------------------------------------------------------------

Reg_CPBR PicoSWIO::get_cpbr() {
  return get(WCH_DM_CPBR);
}

//------------------------------------------------------------------------------

Reg_CFGR PicoSWIO::get_cfgr() {
  return get(WCH_DM_CFGR);
}

//------------------------------------------------------------------------------

Reg_SHDWCFGR PicoSWIO::get_shdwcfgr() {
  return get(WCH_DM_SHDWCFGR);
}

//------------------------------------------------------------------------------

uint32_t PicoSWIO::get_partid() {
  return get(WCH_DM_PART);
}

//------------------------------------------------------------------------------

void PicoSWIO::dump() {
  get_cpbr().dump();
  get_cfgr().dump();
  get_shdwcfgr().dump();
  printf("DM_PARTID = 0x%08x\n", get_partid());
}

//------------------------------------------------------------------------------

const char* PicoSWIO::addr_to_regname(uint8_t addr) {
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

//------------------------------------------------------------------------------

void Reg_CPBR::dump() {
  printf("DM_CPBR = 0x%08x\n", raw);
  printf("  TDIV:%d  SOPN:%d  CHECKSTA:%d  CMDEXTENSTA:%d  OUTSTA:%d  IOMODE:%d  VERSION:%d\n",
    TDIV, SOPN, CHECKSTA, CMDEXTENSTA, OUTSTA, IOMODE, VERSION);
}

void Reg_CFGR::dump() {
  printf("DM_CFGR = 0x%08x\n", raw);
  printf("  TDIVCFG:%d  SOPNCFG:%d  CHECKEN:%d  CMDEXTEN:%d  OUTEN:%d  IOMODECFG:%d  KEY:0x%04x\n",
    TDIVCFG, SOPNCFG, CHECKEN, CMDEXTEN, OUTEN, IOMODECFG, KEY);
}

void Reg_SHDWCFGR::dump() {
  printf("DM_SHDWCFGR = 0x%08x\n", raw);
  printf("  TDIVCFG:%d  SOPNCFG:%d  CHECKEN:%d  CMDEXTEN:%d  OUTEN:%d  IOMODECFG:%d  KEY:0x%04x\n",
    TDIVCFG, SOPNCFG, CHECKEN, CMDEXTEN, OUTEN, IOMODECFG, KEY);
}
