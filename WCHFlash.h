// Small driver to read/write flash in the CH32V003 through the RVD interface

#pragma once
#include <stdint.h>

struct RVDebug;

//------------------------------------------------------------------------------

struct WCHFlash {
  WCHFlash(RVDebug* rvd, int flash_size);
  void reset();

  uint32_t get_flash_base()  { return 0x00000000; }
  int get_flash_size()  { return flash_size; }
  int get_page_size()   { return 64; }
  int get_sector_size() { return 1024; }
  int get_page_count()  { return get_flash_size() / get_page_size(); }

  // Lock/unlock flash. Assume flash always starts locked.
  void lock_flash();
  void unlock_flash();

  // Flash erase, addresses must be aligned
  void wipe_page(uint32_t addr);
  void wipe_sector(uint32_t addr);
  void wipe_chip();

  // Flash write, dest address must be aligned & size must be a multiple of 4
  void write_flash(uint32_t dst_addr, void* data, int size);

  // Debug dump
  void dump();

private:
  void run_flash_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2);

  RVDebug* rvd;
  const int flash_size;
  static const int page_size = 64;
};

//------------------------------------------------------------------------------
