#pragma once
#include <stdint.h>
#include "WCH_Regs.h"

//------------------------------------------------------------------------------

struct SLDebugger {
  void init(int swd_pin, cb_printf print);
  void reset();
  void halt();
  void unhalt(bool reset);
  void step();
  void clear_err();

  // Debugger register access
  uint32_t get_dbg(uint8_t addr);
  void     put_dbg(uint8_t addr, uint32_t data);

  // CPU register access
  uint32_t get_gpr(int index);
  void     put_gpr(int index, uint32_t gpr);

  // CSR access
  uint32_t get_csr(int index);
  void     put_csr(int index, uint32_t csr);

  // Word-size memory access
  uint32_t get_mem(uint32_t addr);
  void     put_mem(uint32_t addr, uint32_t data);

  // Bulk memory access
  void get_block(uint32_t addr, void* data, int size_dwords);
  void put_block(uint32_t addr, void* data, int size_dwords);

  // Flash erase
  void wipe_64  (uint32_t addr);
  void wipe_1024(uint32_t addr);
  void wipe_chip();

  // Flash write
  void write_flash(uint32_t dst_addr, uint32_t* data, int size_dwords);

  // Test stuff
  void print_status();
  void dump_ram();
  void dump_flash();
  bool test_mem();

  //----------------------------------------

private:

  void run_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2);
  void load_prog(uint32_t* prog);

  int swd_pin = -1;
  uint32_t* active_prog = nullptr;

  int reg_count = 16;
  uint32_t regfile[32];

  cb_printf print = nullptr;
};

//------------------------------------------------------------------------------
