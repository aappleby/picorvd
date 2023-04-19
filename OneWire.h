#pragma once
#include <stdint.h>

#define BIT_T0 (1 <<  5)
#define BIT_T1 (1 <<  6)
#define BIT_T2 (1 <<  7)

#define BIT_S0 (1 <<  8)
#define BIT_S1 (1 <<  9)

#define BIT_A0 (1 << 10)
#define BIT_A1 (1 << 11)
#define BIT_A2 (1 << 12)
#define BIT_A3 (1 << 13)
#define BIT_A4 (1 << 14)
#define BIT_A5 (1 << 15)

//------------------------------------------------------------------------------
// Handles the details of talking to the WCH debug module over their
// slightly-odd one wire interface. Nothing here should be chip-specific.

// This class should probably also handle caching debug registers so we don't
// generate excessive bus traffic

struct OneWire {

  void     reset_dbg(int swd_pin);
  uint32_t get_dbg(uint8_t addr);
  void     set_dbg(uint8_t addr, uint32_t data);

  int cmd_count = 0;

  //----------

  void load_prog(const char* name, uint32_t* prog, uint32_t dirty_regs);
  void run_prog_slow();
  void run_prog_fast();

  void halt();
  void resume();
  void step();

  bool is_halted();
  bool hit_breakpoint();
  bool update_halted();

  void reset_cpu();
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
  uint32_t get_mem_u32(uint32_t addr);
  uint16_t get_mem_u16(uint32_t addr);
  uint8_t  get_mem_u8 (uint32_t addr);

  void     set_mem_u32(uint32_t addr, uint32_t data);
  void     set_mem_u16(uint32_t addr, uint16_t data);
  void     set_mem_u8 (uint32_t addr, uint8_t data);

  // Bulk memory access
  void get_block_aligned  (uint32_t addr, void* data, int size);
  void get_block_unaligned(uint32_t addr, void* data, int size);

  void set_block_aligned  (uint32_t addr, void* data, int size);
  void set_block_unaligned(uint32_t addr, void* data, int size);

private:

  static const int pio_sm = 0;

  uint32_t get_mem_u32_aligned(uint32_t addr);
  void     set_mem_u32_aligned(uint32_t addr, uint32_t data);
  void reload_regs();

  int swd_pin = -1;

  // Cached target state, must stay in sync
  int      reg_count = 16;
  uint32_t prog_cache[8];
  uint32_t reg_cache[32];
  uint32_t dirty_regs = 0; // bits are 1 if we modified the reg on device
  uint32_t clean_regs = 0; // bits are 1 if reg_cache[i] is valid
  bool     halted = false;
};

//------------------------------------------------------------------------------
