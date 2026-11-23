// HardBreak - see HardBreak.h.

#include "HardBreak.h"

static const uint32_t BP_EMPTY = 0xFFFFFFFF;

static const int CSR_TSELECT = 0x7A0;
static const int CSR_TDATA1  = 0x7A1;
static const int CSR_TDATA2  = 0x7A2;

// DBGMCU_CR, a WCH CSR (CH32V407 RM 35.2.1): bit 8 IWDG_STOP, bit 9 WWDG_STOP
// stop the watchdog counters while the core is in debug mode. Reset value 0.
static const int CSR_DBGMCU_CR = 0x7C0;
static const uint32_t DBGMCU_WDG_STOP = (1u << 8) | (1u << 9);

// mcontrol (type 2): dmode = 1 (debugger-owned), action = 1 (enter debug
// mode), match = 0 (equal), m = 1, u = 1, execute = 1.
static const uint32_t MCONTROL_EXEC_BREAK =
    (2u << 28) | (1u << 27) | (1u << 12) | (1u << 6) | (1u << 3) | (1u << 2);

//------------------------------------------------------------------------------

HardBreak::HardBreak(RVDebug* rvd) : rvd(rvd) {
  for (int i = 0; i < max_triggers; i++) breakpoints[i] = BP_EMPTY;
}

void HardBreak::init() {
  for (int i = 0; i < max_triggers; i++) breakpoints[i] = BP_EMPTY;
  halted = rvd->get_dmstatus().ALLHALTED;
  seen_resets = rvd->get_reset_count();
}

void HardBreak::dump() {
  printf_b("HardBreak\n");
  printf("  halted %d  triggers %d\n", halted, trigger_count);
  for (int i = 0; i < max_triggers; i++) {
    if (breakpoints[i] != BP_EMPTY) printf("  slot %d: 0x%08x\n", i, (unsigned)breakpoints[i]);
  }
}

//------------------------------------------------------------------------------
// Triggers (hart must be halted for CSR access)

void HardBreak::discover_triggers() {
  trigger_count = 0;
  for (int i = 0; i < max_triggers; i++) {
    rvd->set_csr(CSR_TSELECT, i);
    if (rvd->get_csr(CSR_TSELECT) != (uint32_t)i) break;
    uint32_t tdata1 = rvd->get_csr(CSR_TDATA1);
    if ((tdata1 >> 28) == 0) break;  // type 0: no trigger at this index
    trigger_count++;
  }
  printf_g("HardBreak: %d hardware trigger(s)\n", trigger_count);
}

void HardBreak::arm(int slot) {
  rvd->set_csr(CSR_TSELECT, slot);
  rvd->set_csr(CSR_TDATA1, 0);
  rvd->set_csr(CSR_TDATA2, breakpoints[slot]);
  rvd->set_csr(CSR_TDATA1, MCONTROL_EXEC_BREAK);
}

void HardBreak::disarm(int slot) {
  rvd->set_csr(CSR_TSELECT, slot);
  rvd->set_csr(CSR_TDATA1, 0);
}

void HardBreak::arm_all() {
  for (int i = 0; i < max_triggers; i++) {
    if (breakpoints[i] != BP_EMPTY) arm(i);
  }
}

void HardBreak::disarm_all() {
  for (int i = 0; i < max_triggers; i++) {
    if (breakpoints[i] != BP_EMPTY) disarm(i);
  }
}

//------------------------------------------------------------------------------
// Watchdogs: a watchdog that keeps counting while GDB holds the core halted
// resets the chip under the debugger. Freeze both while halted. Firmware may
// clear the CSR again (it resets to 0), so this runs on every halt and reset.

void HardBreak::freeze_watchdogs() {
  if (dbgmcu_failed) return;
  uint32_t cr = rvd->get_csr(CSR_DBGMCU_CR);
  if (rvd->get_abstractcs().CMDER == 0 && (cr & DBGMCU_WDG_STOP) != DBGMCU_WDG_STOP) {
    rvd->set_csr(CSR_DBGMCU_CR, cr | DBGMCU_WDG_STOP);
  }
  auto cs = rvd->get_abstractcs();
  if (cs.CMDER) {
    printf_r("HardBreak: DBGMCU_CR (CSR 0x7C0) not accessible (CMDERR %d); watchdogs keep running while halted\n",
             (int)cs.CMDER);
    cs.CMDER = 7;  // write 1s to clear
    rvd->set_abstractcs(cs);
    dbgmcu_failed = true;
  }
}

//------------------------------------------------------------------------------
// Run control

// A reset (ours, or one the firmware/watchdog caused that RVDebug has since
// acknowledged) clears the trigger registers and DBGMCU_CR: re-arm both.
// Call with the hart halted.
void HardBreak::check_target_reset() {
  rvd->ack_reset();
  if (rvd->get_reset_count() == seen_resets) return;
  seen_resets = rvd->get_reset_count();
  freeze_watchdogs();
  arm_all();
}

void HardBreak::halt() {
  if (!halted) {
    rvd->halt();
    halted = true;
  }
  check_target_reset();
  freeze_watchdogs();
}

bool HardBreak::resume() {
  if (!halted) return true;
  check_target_reset();

  // A trigger on the current PC would fire again before the instruction runs,
  // so step past it with the triggers disarmed, as the debug spec suggests.
  disarm_all();
  bool stepped = rvd->step();
  arm_all();
  if (!stepped) {
    printf_r("HardBreak: step before resume refused (DMSTATUS 0x%08x)\n", (uint32_t)rvd->get_dmstatus());
  }

  uint32_t dpc = rvd->get_dpc();
  if (has_breakpoint(dpc)) {
    halted = true;
    return false;
  }

  if (!rvd->resume()) {
    printf_r("HardBreak: resume refused (DMSTATUS 0x%08x); target stays halted\n", (uint32_t)rvd->get_dmstatus());
    halted = true;
    return false;
  }
  halted = false;
  return true;
}

void HardBreak::step() {
  check_target_reset();
  disarm_all();
  if (!rvd->step()) {
    printf_r("HardBreak: step refused (DMSTATUS 0x%08x)\n", (uint32_t)rvd->get_dmstatus());
  }
  arm_all();
}

void HardBreak::reset() {
  // RVDebug::reset() keeps the halt request asserted across the reset, so the
  // hart stops at the reset vector before any firmware (or its IWDG) runs.
  rvd->reset();
  halted = true;
  check_target_reset();  // DBGMCU_CR and the triggers were cleared
}

bool HardBreak::is_halted() { return halted; }

void HardBreak::set_dpc(uint32_t pc) { rvd->set_dpc(pc); }

//------------------------------------------------------------------------------
// Breakpoints

int HardBreak::set_breakpoint(uint32_t addr, int size) {
  if (!halted) halt();
  if (trigger_count < 0) discover_triggers();

  if (has_breakpoint(addr)) return 0;

  for (int i = 0; i < trigger_count && i < max_triggers; i++) {
    if (breakpoints[i] == BP_EMPTY) {
      breakpoints[i] = addr;
      arm(i);
      return i;
    }
  }
  printf_r("HardBreak: no free hardware trigger for 0x%08x (%d in use)\n", addr, trigger_count);
  return -1;
}

int HardBreak::clear_breakpoint(uint32_t addr, int size) {
  if (!halted) halt();
  for (int i = 0; i < max_triggers; i++) {
    if (breakpoints[i] == addr) {
      disarm(i);
      breakpoints[i] = BP_EMPTY;
      return i;
    }
  }
  return -1;
}

void HardBreak::clear_all_breakpoints() {
  bool any = false;
  for (int i = 0; i < max_triggers; i++) any |= breakpoints[i] != BP_EMPTY;
  if (!any) return;

  if (!halted) halt();
  disarm_all();
  for (int i = 0; i < max_triggers; i++) breakpoints[i] = BP_EMPTY;
}

bool HardBreak::has_breakpoint(uint32_t addr) {
  for (int i = 0; i < max_triggers; i++) {
    if (breakpoints[i] == addr) return true;
  }
  return false;
}

//------------------------------------------------------------------------------
