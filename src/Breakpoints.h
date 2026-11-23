// Run control and breakpoints, as used by GDBServer and Console.
// Implementations:
//   SoftBreak - patches ebreaks into flash on resume (CH32V003: no hardware
//               triggers, small 64-byte erase pages).

#pragma once
#include <stdint.h>

//------------------------------------------------------------------------------

struct Breakpoints {
  virtual ~Breakpoints() {}
  virtual void init() = 0;
  virtual void dump() = 0;

  virtual void halt() = 0;
  virtual bool resume() = 0;  // false: stepped straight onto a breakpoint, still halted
  virtual void reset() = 0;   // reset the hart and leave it halted
  virtual void step() = 0;
  virtual bool is_halted() = 0;

  virtual void set_dpc(uint32_t pc) = 0;

  // set_breakpoint() returns the slot used, or -1 on failure.
  virtual int  set_breakpoint(uint32_t addr, int size) = 0;
  virtual int  clear_breakpoint(uint32_t addr, int size) = 0;
  virtual void clear_all_breakpoints() = 0;
  virtual bool has_breakpoint(uint32_t addr) = 0;

  // Only meaningful for SoftBreak (console commands).
  virtual void patch_flash() {}
  virtual void unpatch_flash() {}
};

//------------------------------------------------------------------------------
