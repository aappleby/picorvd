// Run control and breakpoints using the RISC-V trigger module (hardware
// breakpoints) instead of patching flash. For WCH QingKe V4 cores (e.g. the
// CH32V4x7), whose smallest flash erase is 4 KB: SoftBreak's page patching
// would be slow and wear the flash there.
//
// The trigger count is discovered from the hart the first time a breakpoint
// is set. If the hart has no usable triggers, set_breakpoint() fails and GDB
// reports the breakpoint as not inserted.
//
// While the hart is halted the watchdogs are frozen through the QingKe V4
// DBGMCU_CR CSR (0x7C0), so a watchdog started by the firmware does not reset
// the chip under the debugger.

#pragma once
#include <stdint.h>
#include "utils.h"
#include "RVDebug.h"
#include "Breakpoints.h"

//------------------------------------------------------------------------------

struct HardBreak : public Breakpoints {
  HardBreak(RVDebug* rvd);
  void init() override;
  void dump() override;

  void halt() override;
  bool resume() override;
  void reset() override;
  void step() override;
  bool is_halted() override;

  void set_dpc(uint32_t pc) override;

  int  set_breakpoint(uint32_t addr, int size) override;
  int  clear_breakpoint(uint32_t addr, int size) override;
  void clear_all_breakpoints() override;
  bool has_breakpoint(uint32_t addr) override;

private:
  static const int max_triggers = 8;

  void discover_triggers();
  void freeze_watchdogs();
  void check_target_reset();
  void arm(int slot);
  void disarm(int slot);
  void arm_all();
  void disarm_all();

  RVDebug* rvd;

  bool halted = false;
  bool dbgmcu_failed = false;            // CSR 0x7C0 not accessible (reported once)
  int  trigger_count = -1;               // -1: not discovered yet
  int  seen_resets = 0;                  // rvd->get_reset_count() when triggers were last armed
  uint32_t breakpoints[max_triggers];    // BP_EMPTY when unused
};

//------------------------------------------------------------------------------
