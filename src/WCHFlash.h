// Interface to a WCH flash controller, driven through the RVD interface.
// Implementations: CH32V003Flash (WCHFlash_CH32V003.h) and CH32V4x7Flash
// (WCHFlash_CH32V4x7.h).

#pragma once
#include <stdint.h>

struct RVDebug;

//------------------------------------------------------------------------------

struct WCHFlash {
  virtual ~WCHFlash() {}
  virtual void reset() = 0;

  virtual uint32_t get_flash_base() { return 0x00000000; }
  virtual int get_flash_size()  = 0;
  virtual int get_page_size()   = 0;  // program granularity
  virtual int get_sector_size() = 0;  // erase granularity of wipe_sector()
  int get_page_count() { return get_flash_size() / get_page_size(); }

  // Lock/unlock flash. Assume flash always starts locked.
  virtual void lock_flash() = 0;
  virtual void unlock_flash() = 0;

  // Flash erase, addresses must be aligned
  virtual void wipe_page(uint32_t addr) = 0;
  virtual void wipe_sector(uint32_t addr) = 0;
  virtual void wipe_chip() = 0;

  // Flash write, dest address must be aligned & size must be a multiple of 4
  virtual void write_flash(uint32_t dst_addr, void* blob, int size) = 0;
  virtual bool verify_flash(uint32_t dst_addr, void* blob, int size) = 0;

  // Debug dump
  virtual void dump() = 0;

  // Set by drivers that detect failures (timeout, write-protect error);
  // GDBServer reports it to GDB and clears it.
  bool error = false;
};

//------------------------------------------------------------------------------
