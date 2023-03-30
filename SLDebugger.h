#pragma once
#include <stdint.h>
#include "utils.h"

//------------------------------------------------------------------------------

struct SLDebugger {
  void init(int swd_pin, putter cb_put);
  void reset();
  void halt();
  void unhalt();
  void reset_cpu();
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
  uint32_t get_mem(uint32_t addr);
  void     set_mem(uint32_t addr, uint32_t data);

  // Bulk memory access
  void get_block(uint32_t addr, void* data, int size_dwords);
  void set_block(uint32_t addr, void* data, int size_dwords);

  void     stream_get_start(uint32_t addr);
  uint32_t stream_get_u32();
  void     stream_get_end();

  void     stream_set_start(uint32_t addr);
  void     stream_set_u32(uint32_t data);
  void     stream_set_end();
  bool first_put = true;


  // Flash erase
  void wipe_page(uint32_t addr);
  void wipe_sector(uint32_t addr);
  void wipe_chip();

  // Flash write
  void write_flash(uint32_t dst_addr, uint32_t* data, int size_dwords);

  // Test stuff
  void print_status();
  void dump_ram();
  void dump_flash();
  bool test_mem();

  void save_reg(int gpr) { regfile[gpr] = get_gpr(gpr); }
  void load_reg(int gpr) { set_gpr(gpr, regfile[gpr]); }

  //----------------------------------------

private:

  bool halted = false;
  bool locked = false;

  void run_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2);
  void load_prog(uint32_t* prog);

  int swd_pin = -1;
  uint32_t* active_prog = nullptr;

  int reg_count = 16;
  uint32_t regfile[32];

  putter cb_put = nullptr;
};

//------------------------------------------------------------------------------
