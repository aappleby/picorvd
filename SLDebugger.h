#pragma once
#include <stdint.h>
#include "utils.h"

//------------------------------------------------------------------------------

struct SLDebugger {
  SLDebugger();

  void init_pio(int swd_pin);
  void reset_dbg();

  void halt();
  void unhalt();
  bool halted();
  bool busy();

  void reset_cpu_and_halt();
  void step();
  void clear_err();

  void lock_flash();
  void unlock_flash();

  // Debugger register access
  uint32_t get_dbg(uint8_t addr);
  void     set_dbg(uint8_t addr, uint32_t data);

  // CPU register access
  uint32_t get_gpr(int index);
  void     set_gpr(int index, uint32_t gpr);

  // CSR access
  uint32_t get_csr(int index);
  void     set_csr(int index, uint32_t csr);

  // Word-size memory access
  uint32_t get_mem_u32_aligned  (uint32_t addr);
  uint32_t get_mem_u32_unaligned(uint32_t addr);
  uint16_t get_mem_u16          (uint32_t addr);
  uint8_t  get_mem_u8           (uint32_t addr);

  void     set_mem_u32_aligned  (uint32_t addr, uint32_t data);
  void     set_mem_u32_unaligned(uint32_t addr, uint32_t data);
  void     set_mem_u16          (uint32_t addr, uint16_t data);
  void     set_mem_u8           (uint32_t addr, uint8_t data);

  void get_block_aligned  (uint32_t addr, void* data, int size);
  void get_block_unaligned(uint32_t addr, void* data, int size);

  void set_block_aligned  (uint32_t addr, void* data, int size);
  void set_block_unaligned(uint32_t addr, void* data, int size);

  // Flash erase, must be aligned
  void wipe_page(uint32_t addr);
  void wipe_sector(uint32_t addr);
  void wipe_chip();

  // Flash write, must be aligned & multiple of 4
  void write_flash2(uint32_t dst_addr, void* data, int size);

  // Breakpoint stuff
  int  set_breakpoint(uint32_t addr);
  int  clear_breakpoint(uint32_t addr);
  void dump_breakpoints();
  void step_over_breakpoint(uint32_t addr);

  void patch_flash();
  void unpatch_flash();

  // Test stuff
  void print_status();
  void dump_ram();
  void dump_flash();
  bool test_mem();
  bool sanity();

  //----------------------------------------

  static const int breakpoint_max = 256;
  static const int flash_size = 16*1024;
  static const int page_size  = 64;
  static const int page_count = flash_size / page_size;

private:

  void save_regs();
  void load_regs();

  void run_flash_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2);
  void load_prog(uint32_t* prog);

  int swd_pin = -1;
  uint32_t* active_prog = nullptr;

  int reg_count = 16;
  uint32_t saved_regs[32];
  uint32_t saved_pc;

  uint32_t breakpoints[breakpoint_max];

  uint8_t  flash_dirty[16384];
  uint8_t  flash_clean[16384];
};

//------------------------------------------------------------------------------
