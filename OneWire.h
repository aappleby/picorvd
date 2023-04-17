#pragma once
#include <stdint.h>

//------------------------------------------------------------------------------
// Handles the details of talking to the WCH debug module over their
// slightly-odd one wire interface. Nothing here should be chip-specific.

// This class should probably also handle caching debug registers so we don't
// generate excessive bus traffic

struct OneWire {

  void init_pio(int swd_pin);
  void reset_dbg();
  uint32_t get_dbg(uint8_t addr);
  void     set_dbg(uint8_t addr, uint32_t data);

  int cmd_count = 0;

  //----------

  void load_prog(uint32_t* prog);
  void run_prog();

  void halt();
  void resume();
  void step();

  bool halted();
  void reset_cpu_and_halt();
  void clear_err();
  bool sanity();
  void status();

  uint32_t get_data0();
  uint32_t get_data1();

  void set_data0(uint32_t d);
  void set_data1(uint32_t d);


  // CPU register access
  uint32_t get_gpr(int index);
  void     set_gpr(int index, uint32_t gpr);

  // CSR access
  uint32_t get_csr(int index);
  void     set_csr(int index, uint32_t csr);

  // Memory access
  uint32_t get_mem_u32_aligned  (uint32_t addr);
  uint32_t get_mem_u32_unaligned(uint32_t addr);
  uint16_t get_mem_u16          (uint32_t addr);
  uint8_t  get_mem_u8           (uint32_t addr);

  void     set_mem_u32_aligned  (uint32_t addr, uint32_t data);
  void     set_mem_u32_unaligned(uint32_t addr, uint32_t data);
  void     set_mem_u16          (uint32_t addr, uint16_t data);
  void     set_mem_u8           (uint32_t addr, uint8_t data);

  // Bulk memory access
  void get_block_aligned  (uint32_t addr, void* data, int size);
  void get_block_unaligned(uint32_t addr, void* data, int size);

  void set_block_aligned  (uint32_t addr, void* data, int size);
  void set_block_unaligned(uint32_t addr, void* data, int size);

  int swd_pin = -1;
  uint32_t* active_prog = nullptr;

  uint32_t prog_cache[8];
};

//------------------------------------------------------------------------------
