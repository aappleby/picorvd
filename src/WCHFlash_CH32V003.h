// Small driver to read/write flash in the CH32V003 through the RVD interface

#pragma once
#include <stdint.h>
#include "WCHFlash.h"

struct RVDebug;

//------------------------------------------------------------------------------

struct CH32V003Flash : public WCHFlash {
  CH32V003Flash(RVDebug* rvd, int flash_size);
  void reset() override;

  int get_flash_size()  override { return flash_size; }
  int get_page_size()   override { return 64; }
  int get_sector_size() override { return 1024; }

  // Lock/unlock flash. Assume flash always starts locked.
  void lock_flash() override;
  void unlock_flash() override;

  // Flash erase, addresses must be aligned
  void wipe_page(uint32_t addr) override;
  void wipe_sector(uint32_t addr) override;
  void wipe_chip() override;

  // Flash write, dest address must be aligned & size must be a multiple of 4
  void write_flash(uint32_t dst_addr, void* blob, int size) override;
  bool verify_flash(uint32_t dst_addr, void* blob, int size) override;

  // Debug dump
  void dump() override;

private:
  void run_flash_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2);

  RVDebug* rvd;
  const int flash_size;
  static const int page_size = 64;
};

//------------------------------------------------------------------------------
