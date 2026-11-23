// Software breakpoint support for WCH MCUs.
// Patches flash to insert breakpoints on resume, unpatches flash on halt.

// Includes a small optimization to prevent excessive patch/unpatching - if the
// next instruction is a breakpoint when we're about to resume the CPU, we skip
// the patch/unpatch, step to the breakpoint, and just leave the CPU halted.

#pragma once
#include <stdint.h>
#include "utils.h"
#include "RVDebug.h"
#include "WCHFlash.h"
#include "Breakpoints.h"

//------------------------------------------------------------------------------

struct SoftBreak : public Breakpoints {
  SoftBreak(RVDebug* rvd, WCHFlash* flash);
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
  void patch_flash() override;
  void unpatch_flash() override;

  //----------------------------------------

private:

  RVDebug* rvd;
  WCHFlash* flash;

  bool halted;

  int breakpoint_count;
  uint32_t* breakpoints;

  uint8_t*  flash_clean; // Buffer for cached flash contents.
  uint8_t*  flash_dirty; // Buffer for cached flash contents w/ breakpoints inserted

  uint8_t*  break_map; // Number of breakpoints set, per page
  uint8_t*  flash_map; // Number of breakpoints written to device flash, per page.
  uint8_t*  dirty_map; // Nonzero if the flash page does not match our flash_dirty copy.
};

//------------------------------------------------------------------------------
