#pragma once
#include <stdint.h>
#include "WCH_Regs.h"

//------------------------------------------------------------------------------

struct SLDebugger {
  void init(int swd_pin, cb_printf print);
  void reset();
  void halt();
  void unhalt(bool reset);

  // Raw access to debug registers
  uint32_t get_dbg (uint8_t addr);
  void     get_dbgp(uint8_t addr, void* out);
  void     put_dbg (uint8_t addr, uint32_t data);

  void     load_prog(uint32_t* prog);
  void     save_regs();
  void     load_regs();

  uint32_t get_mem32(uint32_t addr);
  void     get_mem32p(uint32_t addr, void* data);
  void     get_block32(uint32_t addr, void* data, int size_dwords);

  void     put_mem16(uint32_t addr, uint16_t data);
  void     put_mem32(uint32_t addr, uint32_t data);
  void     put_mem32p(uint32_t addr, void* data);
  void     put_block32(uint32_t addr, void* data, int size_dwords);

  uint32_t get_gpr(int index);
  void     put_gpr(int index, uint32_t gpr);

  uint32_t get_csr(int index);
  void     put_csr(int index, uint32_t csr);

  void     get_csrp(int index, void* data);
  void     put_csrp(int index, void* csr);

  //----------

  Reg_DCSR get_dcsr() { Reg_DCSR r; get_csrp(0x7B0, &r); return r; }
  uint32_t get_dpc()  { uint32_t r; get_csrp(0x7B1, &r); return r; }
  uint32_t get_ds0()  { uint32_t r; get_csrp(0x7B2, &r); return r; }
  uint32_t get_ds1()  { uint32_t r; get_csrp(0x7B3, &r); return r; }

  Reg_DMSTATUS   get_dmstatus() { Reg_DMSTATUS r;   r.raw = get_dbg(ADDR_DMSTATUS);   return r; }
  Reg_ABSTRACTCS get_abstatus() { Reg_ABSTRACTCS r; r.raw = get_dbg(ADDR_ABSTRACTCS); return r; }

  //----------

  bool test_mem();
  void step();

  int swd_pin = -1;
  uint32_t* active_prog = nullptr;

  int  reg_count = 16;
  uint32_t regfile[32];

  cb_printf print = nullptr;

  void wipe_64   (uint32_t addr);
  void wipe_1024 (uint32_t addr);
  void wipe_chip ();

  void run_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2);
  void print_status();
  void dump_ram();
  void dump_flash();
  void write_flash(uint32_t dst_addr, uint32_t* data, int size_dwords);
};

//------------------------------------------------------------------------------
