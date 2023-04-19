#include "OneWire.h"
#include "utils.h"
#include "WCH_Regs.h"
#include "build/singlewire.pio.h"
#include "string.h"

#define DUMP_COMMANDS

#if 0
  // If we attach while running, resume.
  //if (!halted) {
  //  wire.resume();
  //}

  /*
  if (halted) {
    const char* halt_cause[] = {
      "<not halted>",
      "ebreak",
      "trigger",
      "halt request",
      "step",
      "halt on reset",
      "<invalid6>",
      "<invalid7>",
    };
    printf("SLDebugger::attach() - CPU is halted, cause = %s\n", halt_cause[dcsr_on_attach.CAUSE]);
  }
  else {
    printf("SLDebugger::attach() - CPU is running\n");
  }
  */
#endif

void busy_wait(int count) {
  volatile int c = count;
  while (c) c = c - 1;
}

//------------------------------------------------------------------------------

void OneWire::reset_dbg(int swd_pin) {
  CHECK(swd_pin != -1);
  this->swd_pin = swd_pin;

  // Configure GPIO
  gpio_set_drive_strength(swd_pin, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate     (swd_pin, GPIO_SLEW_RATE_SLOW);
  gpio_set_function      (swd_pin, GPIO_FUNC_PIO0);

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
  sm_config_set_out_pins    (&c, swd_pin, 1);
  sm_config_set_in_pins     (&c, swd_pin);
  sm_config_set_set_pins    (&c, swd_pin, 1);
  sm_config_set_sideset_pins(&c, swd_pin);
  sm_config_set_out_shift   (&c, /*shift_right*/ false, /*autopull*/ false, /*pull_threshold*/ 32);
  sm_config_set_in_shift    (&c, /*shift_right*/ false, /*autopush*/ true,  /*push_threshold*/ 32);

  // 125 mhz / 12 = 96 nanoseconds per tick, close enough to 100 ns.
  sm_config_set_clkdiv      (&c, 12);

  pio_sm_init       (pio0, pio_sm, pio_offset, &c);
  pio_sm_set_pins   (pio0, pio_sm, 0);
  pio_sm_set_enabled(pio0, pio_sm, true);

  // Grab pin and send an 8 usec low pulse to reset debug module
  // If we use the sdk functions to do this we get jitter :/
  sio_hw->gpio_clr    = (1 << swd_pin);
  sio_hw->gpio_oe_set = (1 << swd_pin);
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  busy_wait(100); // ~8 usec
  sio_hw->gpio_oe_clr = (1 << swd_pin);
  iobank0_hw->io[swd_pin].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // Enable debug output pin on target
  set_dbg(WCH_DM_SHDWCFGR, 0x5AA50400);
  set_dbg(WCH_DM_CFGR,     0x5AA50400);

  // Reset debug module on target
  set_dbg(DM_DMCONTROL, 0x00000000);
  set_dbg(DM_DMCONTROL, 0x00000001);

  // Reset cached state
  for (int i = 0; i < 8; i++) prog_cache[i] = 0xDEADBEEF;
  for (int i = 0; i < reg_count; i++) reg_cache[i] = 0xDEADBEEF;
  dirty_regs = 0;
  clean_regs = 0;
  halted = Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED;

  // Halt momentarily if needed so we can enable breakpoints in DCSR
  bool was_halted = halted;
  if (!was_halted) halt();
  auto dcsr = Csr_DCSR(get_csr(CSR_DCSR));
  dcsr.EBREAKM = 1;
  dcsr.EBREAKS = 1;
  dcsr.EBREAKU = 1;
  dcsr.STEPIE = 0;
  dcsr.STOPCOUNT = 1;
  dcsr.STOPTIME = 1;
  set_csr(CSR_DCSR, dcsr);
  if (!was_halted) resume();
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
#ifdef DUMP_COMMANDS
  printf("get_dbg %15s 0x%08x\n", addr_to_regname(addr), data);
#endif
  //for(volatile int i = 0; i < 100000; i++);
  return data;
}

//------------------------------------------------------------------------------

void OneWire::set_dbg(uint8_t addr, uint32_t data) {
  cmd_count++;
#ifdef DUMP_COMMANDS
  printf("set_dbg %15s 0x%08x\n", addr_to_regname(addr), data);
#endif
  pio_sm_put_blocking(pio0, 0, ((~addr) << 1) | 0);
  pio_sm_put_blocking(pio0, 0, ~data);
}

//------------------------------------------------------------------------------

void OneWire::halt() {
  printf("OneWire::halt()\n");
  set_dbg(DM_DMCONTROL, 0x80000001);
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED);
  set_dbg(DM_DMCONTROL, 0x00000001);

  halted = true;

  printf("OneWire::halt() done\n");
}

//------------------------------------------------------------------------------

void OneWire::resume() {
  printf("OneWire::resume()\n");
  set_dbg(DM_DMCONTROL, 0x40000001);
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLRESUMEACK);
  set_dbg(DM_DMCONTROL, 0x00000001);

  halted = false;
  clean_regs = 0;

  printf("OneWire::resume() done\n");
}

//------------------------------------------------------------------------------

void OneWire::step() {
  printf("OneWire::step()\n");
  
  Csr_DCSR dcsr = get_csr(CSR_DCSR);
  dcsr.STEP = 1;
  set_csr(CSR_DCSR, dcsr);

  set_dbg(DM_DMCONTROL, 0x40000001);
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLRESUMEACK); // we might be able to skip this check?
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED);
  set_dbg(DM_DMCONTROL, 0x00000001);

  halted = true;
  clean_regs = 0;

  dcsr.STEP = 0;
  set_csr(CSR_DCSR, dcsr);

  printf("OneWire::step() done\n");
}

//------------------------------------------------------------------------------

bool OneWire::is_halted() {
  return halted;
}

bool OneWire::hit_breakpoint() {
  bool new_halted = Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED;
  if (!halted && new_halted) {
    halted = true;
    return true;
  }
  else {
    return false;
  }
}

bool OneWire::update_halted() {
  halted = Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED;
  return halted;
}

//------------------------------------------------------------------------------

void OneWire::reset_cpu() {
  printf("reset_cpu()");

  // Halt and leave halt request set
  set_dbg(DM_DMCONTROL, 0x80000001);
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED);

  // Set reset request
  set_dbg(DM_DMCONTROL, 0x80000003);
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHAVERESET);

  // Clear reset request and hold halt request
  set_dbg(DM_DMCONTROL, 0x80000001);
  // this busywait seems to be required or we hang
  while (!Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHALTED); 

  // Clear HAVERESET
  set_dbg(DM_DMCONTROL, 0x90000001);
  while (Reg_DMSTATUS(get_dbg(DM_DMSTATUS)).ALLHAVERESET);

  // Clear halt request
  set_dbg(DM_DMCONTROL, 0x00000001);

  // Resetting the CPU also resets DCSR, redo it.
  auto dcsr = Csr_DCSR(get_csr(CSR_DCSR));
  dcsr.EBREAKM = 1;
  dcsr.EBREAKS = 1;
  dcsr.EBREAKU = 1;
  dcsr.STEPIE = 0;
  dcsr.STOPCOUNT = 1;
  dcsr.STOPTIME = 1;
  set_csr(CSR_DCSR, dcsr);

  // Reset cached state
  for (int i = 0; i < reg_count; i++) reg_cache[i] = 0xDEADBEEF;
  dirty_regs = 0;
  clean_regs = 0;
  halted = true;
}

//------------------------------------------------------------------------------

void OneWire::load_prog(const char* name, uint32_t* prog, uint32_t dirty_regs) {
  printf("load_prog %s 0x%08x\n", name, dirty_regs);

  for (int i = 0; i < reg_count; i++) {
    if (dirty_regs & (1 << i)) {
      if (clean_regs & (1 << i)) {
        // already got a clean copy
      }
      else {
        reg_cache[i] = get_gpr(i);
        clean_regs |= (1 << i);
      }
    }
  }

  this->dirty_regs |= dirty_regs;

  for (int i = 0; i < 8; i++) {
    if (prog_cache[i] != prog[i]) {
      set_dbg(0x20 + i, prog[i]);
      prog_cache[i] = prog[i];
    }
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
  Reg_COMMAND cmd;
  cmd.REGNO      = 0x1000 | index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(DM_COMMAND, cmd);
  auto result = get_dbg(DM_DATA0);
  return result;
}

//------------------------------------------------------------------------------

void OneWire::set_gpr(int index, uint32_t gpr) {
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
  }
}

//------------------------------------------------------------------------------

#if 0
void OneWire::save_regs() {
  printf("OneWire::save_regs\n");
  for (int i = 0; i < reg_count; i++) {
    reg_cache[i] = get_gpr(i);
  }
  dirty_regs = 0;
}

void OneWire::load_regs() {
  printf("OneWire::load_regs 0x%08x\n", dirty_regs);
  for (int i = 0; i < reg_count; i++) {
    uint32_t actual = get_gpr(i);

    bool dirty1 = (actual != reg_cache[i]);
    bool dirty2 = (dirty_regs & (1 << i));

    if (dirty1 != dirty2) {
      printf("Dirty flags for GPR %02d invalid - dirty1 %d dirty2 %d\n", i, dirty1, dirty2);
      CHECK(false);
    }

    set_gpr(i, reg_cache[i]);
  }
  dirty_regs = 0;
}
#endif

void OneWire::reload_regs() {
  for (int i = 0; i < reg_count; i++) {
    if (dirty_regs & (1 << i)) {
      if (clean_regs & (1 << i)) {
        set_gpr(i, reg_cache[i]);
      }
      else {
        CHECK(false, "GPR %d is dirity and we dont' have a saved copy!\n", i);
      }
    }
  }

  dirty_regs = 0;
}

//------------------------------------------------------------------------------

uint32_t OneWire::get_csr(int index) {
  Reg_COMMAND cmd;
  cmd.REGNO      = index;
  cmd.WRITE      = 0;
  cmd.TRANSFER   = 1;
  cmd.POSTEXEC   = 0;
  cmd.AARPOSTINC = 0;
  cmd.AARSIZE    = 2;
  cmd.CMDTYPE    = 0;

  set_dbg(DM_COMMAND, cmd);
  auto result = get_dbg(DM_DATA0);
  return result;
}

//------------------------------------------------------------------------------

void OneWire::set_csr(int index, uint32_t data) {
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
}

//------------------------------------------------------------------------------

void OneWire::run_prog_slow() {
  const uint32_t cmd_runprog = 0x00040000;
  set_dbg(DM_COMMAND, cmd_runprog);
  // we definitely need this busywait
  while (Reg_ABSTRACTCS(get_dbg(DM_ABSTRACTCS)).BUSY);
}

void OneWire::run_prog_fast() {
  const uint32_t cmd_runprog = 0x00040000;
  set_dbg(DM_COMMAND, cmd_runprog);

  // It takes 40 usec to do _anything_ over the debug interface, so if the
  // program is "fast" then we should _never_ see BUSY... right?
  CHECK(!Reg_ABSTRACTCS(get_dbg(DM_ABSTRACTCS)).BUSY);
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

uint32_t OneWire::get_mem_u32(uint32_t addr) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  auto data_lo = get_mem_u32_aligned(addr_lo);

  if (offset == 0) return data_lo;

  auto data_hi = get_mem_u32_aligned(addr_hi);
  
  return (data_lo >> (offset * 8)) | (data_hi << (32 - offset * 8));
}

//------------------------------------------------------------------------------

uint16_t OneWire::get_mem_u16(uint32_t addr) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  uint32_t data_lo = get_mem_u32_aligned(addr_lo);

  if (offset < 3) return data_lo >> (offset * 8);

  uint32_t data_hi = get_mem_u32_aligned(addr_hi);

  return (data_lo >> 24) | (data_hi << 8);
}

//------------------------------------------------------------------------------

uint8_t OneWire::get_mem_u8(uint32_t addr) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  uint32_t data_lo = get_mem_u32_aligned(addr_lo);

  return data_lo >> (offset * 8);
}

//------------------------------------------------------------------------------

void OneWire::set_mem_u32(uint32_t addr, uint32_t data) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 4) & ~3;

  if (offset == 0) {
    set_mem_u32_aligned(addr_lo, data);
    return;
  }
  
  uint32_t data_lo = get_mem_u32_aligned(addr_lo);
  uint32_t data_hi = get_mem_u32_aligned(addr_hi);
  
  if (offset == 1) {
    data_lo &= 0x000000FF;
    data_hi &= 0xFFFFFF00;
    data_lo |= data << 8;
    data_hi |= data >> 24;
  }
  else if (offset == 2) {
    data_lo &= 0x0000FFFF;
    data_hi &= 0xFFFF0000;
    data_lo |= data << 16;
    data_hi |= data >> 16;
  }
  else if (offset == 3) {
    data_lo &= 0x00FFFFFF;
    data_hi &= 0xFF000000;
    data_lo |= data << 24;
    data_hi |= data >> 8;
  }

  set_mem_u32_aligned(addr_lo, data_lo);
  set_mem_u32_aligned(addr_hi, data_hi);
}

//------------------------------------------------------------------------------

void OneWire::set_mem_u16(uint32_t addr, uint16_t data) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  uint32_t data_lo = get_mem_u32_aligned(addr_lo);

  if (offset == 0) {
    data_lo &= 0xFFFF0000;
    data_lo |= data << 0;
  }
  else if (offset == 1) {
    data_lo &= 0xFF0000FF;
    data_lo |= data << 8;
  }
  else if (offset == 2) {
    data_lo &= 0x0000FFFF;
    data_lo |= data << 16;
  }
  else if (offset == 3) {
    data_lo &= 0x00FFFFFF;
    data_lo |= data << 24;
  }

  set_mem_u32_aligned(addr_lo, data_lo);

  if (offset == 3) {
    uint32_t data_hi = get_mem_u32_aligned(addr_hi);
    data_hi &= 0xFFFFFF00;
    data_hi |= data >> 8;
    set_mem_u32_aligned(addr_hi, data_hi);
  }
}

//------------------------------------------------------------------------------

void OneWire::set_mem_u8(uint32_t addr, uint8_t data) {
  auto offset  = addr & 3;
  auto addr_lo = (addr + 0) & ~3;

  uint32_t data_lo = get_mem_u32_aligned(addr_lo);

  if (offset == 0) {
    data_lo = (data_lo & 0xFFFFFF00) | (data << 0);
  }
  else if (offset == 1) {
    data_lo = (data_lo & 0xFFFF00FF) | (data << 8);
  }
  else if (offset == 2) {
    data_lo = (data_lo & 0xFF00FFFF) | (data << 16);
  }
  else if (offset == 3) {
    data_lo = (data_lo & 0x00FFFFFF) | (data << 24);
  }

  set_mem_u32_aligned(addr_lo, data_lo);
}

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

  load_prog("get_mem_u32", prog_get_mem, BIT_A0 | BIT_A1);
  set_data1(addr);
  run_prog_fast();

  auto result = get_data0();

  return result;
}

//------------------------------------------------------------------------------

void OneWire::set_mem_u32_aligned(uint32_t addr, uint32_t data) {
  if (addr & 3) {
    printf("OneWire::set_mem_u32_aligned - Bad alignment 0x%08x\n", addr);
    return;
  }

  static uint32_t prog_set_mem[8] = {
    0xe0000537, // lui  a0, 0xE0000
    0x0f852583, // lw   a1, 0x0F8(a0)
    0x0f452503, // lw   a0, 0x0F4(a0)
    0x00a5a023, // sw   a0, 0x000(a1)
    0x00100073, // ebreak
    0x00100073, // ebreak
    0x00100073, // ebreak
    0x00100073, // ebreak
  };

  load_prog("set_mem_u32", prog_set_mem, BIT_A0 | BIT_A1);

  set_data0(data);
  set_data1(addr);
  run_prog_fast();
}

//------------------------------------------------------------------------------
// FIXME we should handle invalid address ranges somehow...

void OneWire::get_block_aligned(uint32_t addr, void* dst, int size_bytes) {
  CHECK((addr & 3) == 0, "OneWire::get_block_aligned() bad address");
  CHECK((size_bytes & 3) == 0, "OneWire::get_block_aligned() bad size");

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

  load_prog("get_block_aligned", prog_get_block_aligned, BIT_A0 | BIT_A1);
  set_data1(addr);

  uint32_t* cursor = (uint32_t*)dst;
  for (int i = 0; i < size_bytes / 4; i++) {
    run_prog_fast();
    cursor[i] = get_data0();
  }
}

//------------------------------------------------------------------------------

void OneWire::get_block_unaligned(uint32_t addr, void* dst, int size_bytes) {
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

  load_prog("get_block_unaligned", prog_get_block_unaligned, BIT_A0 | BIT_A1);
  set_data1(addr);

  uint8_t* cursor = (uint8_t*)dst;
  for (int i = 0; i < size_bytes; i++) {
    run_prog_fast();
    cursor[i] = get_data0();
  }
}

//------------------------------------------------------------------------------

void OneWire::set_block_aligned(uint32_t addr, void* src, int size_bytes) {
  CHECK((addr & 3) == 0);
  CHECK((size_bytes & 3) == 0);

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


  load_prog("set_block_aligned", prog_set_block_aligned, BIT_A0 | BIT_A1);
  set_data1(addr);

  uint32_t* cursor = (uint32_t*)src;
  for (int i = 0; i < size_bytes / 4; i++) {
    set_data0(*cursor++);
    run_prog_fast();
  }
}

//------------------------------------------------------------------------------

void OneWire::set_block_unaligned(uint32_t addr, void* src, int size_bytes) {
  static uint32_t prog_set_block_unaligned[8] = {
    0xe0000537, // lui    a0, 0xE0000
    0x0f852583, // lw     a1, 0x0F8(a0)
    0x0f452503, // lw     a0, 0x0F4(a0)
    0x00a58023, // sb     a0, 0x000(a1)
    0x00158593, // addi   a1, a1, 1
    0xe0000537, // lui    a0, 0xE0000
    0x0eb52c23, // sw     a1, 0x0F8(a0)
    0x00100073, // ebreak
  };

  load_prog("set_block_unaligned", prog_set_block_unaligned, BIT_A0 | BIT_A1);
  set_data1(addr);

  uint8_t* cursor = (uint8_t*)src;
  for (int i = 0; i < size_bytes; i++) {
    set_data0(*cursor++);
    run_prog_fast();
  }
}

//------------------------------------------------------------------------------
