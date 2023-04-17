#pragma once
#include <stdint.h>
#include "utils.h"
#include "WCH_Regs.h"
#include "OneWire.h"

// FIXME all progs should save/restore regs so we don't have to do it on every
// halt/resume

//------------------------------------------------------------------------------

struct SLDebugger {
  SLDebugger();

  void reset_dbg();

  void attach();
  void detach();

  void patch_dcsr();

  void halt();
  void async_halted();
  bool resume2();
  
  void reset_cpu_and_halt();
  void step();

  void lock_flash();
  void unlock_flash();

  // Flash erase, must be aligned
  void wipe_page(uint32_t addr);
  void wipe_sector(uint32_t addr);
  void wipe_chip();

  // Flash write, must be aligned & multiple of 4
  void write_flash2(uint32_t dst_addr, void* data, int size);

  // Breakpoint stuff
  int  set_breakpoint(uint32_t addr, int size);
  int  clear_breakpoint(uint32_t addr, int size);
  void dump_breakpoints();

  bool has_breakpoint(uint32_t addr) {
    for (int i = 0; i < breakpoint_max; i++) {
      if (breakpoints[i] == addr) return true;
    }
    return false;
  }

  void patch_flash();
  void unpatch_flash();

  void patch_page(int page);
  void unpatch_page(int page);

  // Test stuff
  void status();
  void dump_ram();
  void dump_flash();
  bool test_mem();
  bool sanity();

  //----------------------------------------

  static const int breakpoint_max = 256;
  static const int flash_size = 16*1024;
  static const int page_size  = 64;
  static const int page_count = flash_size / page_size;

//private:

  void save_regs();
  void load_regs();

  void run_flash_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2);
  void load_prog(uint32_t* prog);

  static constexpr uint32_t BP_EMPTY = 0xDEADBEEF;

  OneWire wire;

  bool attached = false;
  bool halted = false;
  int  halt_cause = 0;

  // State of the chip when we attached the debugger
  Reg_DMSTATUS old_dmstatus;
  Csr_DCSR     dcsr_on_attach;

  int      reg_count = 16;
  uint32_t saved_regs[32];
  uint32_t saved_pc;

  int      breakpoint_count;
  uint32_t breakpoints[breakpoint_max];

  uint8_t  break_map[page_count]; // Number of breakpoints set, per page
  uint8_t  flash_map[page_count]; // Number of breakpoints written to device flash, per page.
  uint8_t  dirty_map[page_count]; // Nonzero if the flash page does not match our flash_dirty copy.

  uint8_t  flash_dirty[16384];
  uint8_t  flash_clean[16384];
};

//------------------------------------------------------------------------------
