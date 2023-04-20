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

//------------------------------------------------------------------------------

struct SoftBreak {
  SoftBreak(RVDebug* rvd, WCHFlash* flash);
  void reset();
  void update(); // Call this regularly to see if we've hit a breakpoint.

  void halt();
  void async_halted();
  bool resume();
  void set_dpc(uint32_t pc);

  void reset_cpu();
  void step();
  bool is_halted();

  int  set_breakpoint(uint32_t addr, int size);
  int  clear_breakpoint(uint32_t addr, int size);
  void clear_all_breakpoints();
  bool has_breakpoint(uint32_t addr);

  void dump();

  //----------------------------------------

private:

  void patch_flash();
  void unpatch_flash();

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
