#pragma once
#include <stdint.h>
#include "utils.h"
#include "WCH_Regs.h"
#include "OneWire.h"

// FIXME all progs should save/restore regs so we don't have to do it on every
// halt/resume

static const int ch32v003_flash_size = 16*1024;
static const int ch32v003_page_size  = 64;
static const int ch32v003_page_count = ch32v003_flash_size / ch32v003_page_size;

//------------------------------------------------------------------------------

struct SLDebugger {
  SLDebugger();

  void reset_dbg(int swd_pin);

  void halt();
  void async_halted();
  bool resume();
  
  void reset_cpu();
  void step();

  void lock_flash();
  void unlock_flash();

  // Flash erase, must be aligned
  void wipe_page(uint32_t addr);
  void wipe_sector(uint32_t addr);
  void wipe_chip();

  // Flash write, must be aligned & multiple of 4
  void write_flash(uint32_t dst_addr, void* data, int size);

  // Breakpoint stuff
  int  set_breakpoint(uint32_t addr, int size);
  int  clear_breakpoint(uint32_t addr, int size);
  void clear_all_breakpoints();
  void dump_breakpoints();
  bool has_breakpoint(uint32_t addr);

  void patch_flash();
  void unpatch_flash();

  void patch_page(int page);
  void unpatch_page(int page);

  // Test stuff
  void status();
  void dump_ram();
  void dump_flash();

  bool is_halted()      { return wire.is_halted(); }
  bool hit_breakpoint() { return wire.hit_breakpoint(); }

  //----------------------------------------

private:

  static const int breakpoint_max = 256;

  void run_flash_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2);
  void load_prog(uint32_t* prog);

  static constexpr uint32_t BP_EMPTY = 0xDEADBEEF;

  OneWire  wire;
  int      breakpoint_count;
  uint32_t breakpoints[breakpoint_max];

  uint8_t  break_map[ch32v003_page_count]; // Number of breakpoints set, per page
  uint8_t  flash_map[ch32v003_page_count]; // Number of breakpoints written to device flash, per page.
  uint8_t  dirty_map[ch32v003_page_count]; // Nonzero if the flash page does not match our flash_dirty copy.

  uint8_t  flash_dirty[16384];
  uint8_t  flash_clean[16384];
};

//------------------------------------------------------------------------------
