// Flash driver for the CH32V4x7 (CH32V407/V417) flash controller, driven
// through the RVD interface.
//
// Differences from the CH32V003 handled here (from the WCH CH32V407 EVT,
// ch32v4x7_flash.c and ch32v4x7.h):
//   - flash programmed at 0x08000000 (executed from the 0x00000000 alias);
//   - 256-byte program page (CTLR.PAGE_PG, words stored directly, then
//     CTLR.PG_STRT), STATR.WRBSY polled after every word;
//   - smallest erase is a 4 KB sector (CTLR.PER); no 256-byte page erase;
//   - CTLR also holds the flash access clock (SCKMOD) and read mode bits, so
//     every CTLR update is read-modify-write.
//
// Only standard debug-module memory accesses are used (no program-buffer
// code), so it is simple rather than fast.

#pragma once
#include <stdint.h>
#include "WCHFlash.h"

struct RVDebug;

//------------------------------------------------------------------------------

struct CH32V4x7Flash : public WCHFlash {
  CH32V4x7Flash(RVDebug* rvd, int flash_size);
  void reset() override;

  int get_flash_size()  override { return flash_size; }
  int get_page_size()   override { return 256; }   // program granularity
  int get_sector_size() override { return 4096; }  // erase granularity

  // Unlock only writes keys to a locked controller.
  void lock_flash() override;
  void unlock_flash() override;

  // wipe_page() cannot erase 256 bytes on this part; it erases the enclosing
  // 4 KB sector when given the sector's first page and fails otherwise. GDB
  // never calls it because the memory map advertises 4 KB blocks.
  void wipe_page(uint32_t addr) override;
  void wipe_sector(uint32_t addr) override;
  void wipe_chip() override;

  // Dest address must be page aligned & size a multiple of 4. A short final
  // page is padded with 0xFF.
  void write_flash(uint32_t dst_addr, void* blob, int size) override;
  bool verify_flash(uint32_t dst_addr, void* blob, int size) override;

  void dump() override;

private:
  uint32_t read_ctlr();
  void     write_ctlr(uint32_t value);
  bool     wait_statr_clear(uint32_t mask, uint32_t timeout_us);
  void     erase(uint32_t addr, uint32_t mode_bit);
  void     write_page(uint32_t page_addr, const uint32_t* words);

  RVDebug* rvd;
  const int flash_size;
};

//------------------------------------------------------------------------------
