#pragma once
#include <stdint.h>

struct Bus {
  virtual uint32_t get(uint32_t addr) = 0;
  virtual void     put(uint32_t addr, uint32_t data) = 0;

  /*
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
  */
};
