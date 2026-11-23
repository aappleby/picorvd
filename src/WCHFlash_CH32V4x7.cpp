// Flash driver for the CH32V4x7 - see WCHFlash_CH32V4x7.h.
//
// Register sequences follow the WCH CH32V407 EVT (Peripheral/src/
// ch32v4x7_flash.c: FLASH_Unlock_Fast, FLASH_ErasePage, FLASH_EraseAllPages,
// FLASH_ROM_WRITE). Every register access is a debug-module memory access, so
// this is simple rather than fast: about two DMI memory operations per word.

#include "WCHFlash_CH32V4x7.h"

#include <string.h>

#include "RVDebug.h"
#include "utils.h"

#include "pico/stdlib.h"

static const uint32_t FLASH_PROG_BASE   = 0x08000000; // program/erase alias

static const uint32_t ADDR_FLASH_KEYR   = 0x40022004;
static const uint32_t ADDR_FLASH_STATR  = 0x4002200C;
static const uint32_t ADDR_FLASH_CTLR   = 0x40022010;
static const uint32_t ADDR_FLASH_ADDR   = 0x40022014;
static const uint32_t ADDR_FLASH_OBR    = 0x4002201C;
static const uint32_t ADDR_FLASH_WPR    = 0x40022020;
static const uint32_t ADDR_FLASH_MKEYR  = 0x40022024;

static const uint32_t FLASH_KEY1 = 0x45670123;
static const uint32_t FLASH_KEY2 = 0xCDEF89AB;

// STATR
static const uint32_t SR_BSY      = 1u << 0;
static const uint32_t SR_WRBSY    = 1u << 1;
static const uint32_t SR_WRPRTERR = 1u << 4;
static const uint32_t SR_EOP      = 1u << 5;

// CTLR
static const uint32_t CR_PER     = 1u << 1;   // 4 KB sector erase
static const uint32_t CR_MER     = 1u << 2;   // mass erase
static const uint32_t CR_STRT    = 1u << 6;
static const uint32_t CR_LOCK    = 1u << 7;
static const uint32_t CR_FLOCK   = 1u << 15;  // fast (page) mode lock
static const uint32_t CR_PAGE_PG = 1u << 16;  // 256-byte page program
static const uint32_t CR_PG_STRT = 1u << 21;  // page program start

// Timeouts. The datasheet values are not in the EVT; these are generous.
static const uint32_t TIMEOUT_WORD_US   = 10 * 1000;
static const uint32_t TIMEOUT_PAGE_US   = 100 * 1000;
static const uint32_t TIMEOUT_SECTOR_US = 500 * 1000;
static const uint32_t TIMEOUT_CHIP_US   = 30 * 1000 * 1000;

static const int PAGE_SIZE = 256;
static const int PAGE_WORDS = PAGE_SIZE / 4;
static const int SECTOR_SIZE = 4096;

//------------------------------------------------------------------------------

CH32V4x7Flash::CH32V4x7Flash(RVDebug* rvd, int flash_size) : rvd(rvd), flash_size(flash_size) {}

void CH32V4x7Flash::reset() { error = false; }

uint32_t CH32V4x7Flash::read_ctlr() { return rvd->get_mem_u32(ADDR_FLASH_CTLR); }

void CH32V4x7Flash::write_ctlr(uint32_t value) {
  // Never write 1 to LOCK/FLOCK by accident: they are set-only and re-lock
  // the controller. Callers that want to lock use lock_flash().
  rvd->set_mem_u32(ADDR_FLASH_CTLR, value & ~(CR_LOCK | CR_FLOCK));
}

bool CH32V4x7Flash::wait_statr_clear(uint32_t mask, uint32_t timeout_us) {
  uint32_t start = time_us_32();
  while (rvd->get_mem_u32(ADDR_FLASH_STATR) & mask) {
    if ((time_us_32() - start) > timeout_us) {
      printf_r("CH32V4x7Flash: timeout waiting for STATR & 0x%02x\n", mask);
      error = true;
      return false;
    }
  }
  return true;
}

//------------------------------------------------------------------------------

void CH32V4x7Flash::lock_flash() {
  uint32_t ctlr = read_ctlr();
  rvd->set_mem_u32(ADDR_FLASH_CTLR, ctlr | CR_FLOCK);
  rvd->set_mem_u32(ADDR_FLASH_CTLR, ctlr | CR_FLOCK | CR_LOCK);
}

void CH32V4x7Flash::unlock_flash() {
  // Writing the key sequence to an already unlocked controller may be treated
  // as a wrong sequence (as on the STM32 FPEC this controller resembles), so
  // only unlock what is locked.
  uint32_t ctlr = read_ctlr();
  if (ctlr & CR_LOCK) {
    rvd->set_mem_u32(ADDR_FLASH_KEYR, FLASH_KEY1);
    rvd->set_mem_u32(ADDR_FLASH_KEYR, FLASH_KEY2);
  }
  if (ctlr & CR_FLOCK) {
    rvd->set_mem_u32(ADDR_FLASH_MKEYR, FLASH_KEY1);
    rvd->set_mem_u32(ADDR_FLASH_MKEYR, FLASH_KEY2);
  }
  ctlr = read_ctlr();
  if (ctlr & (CR_LOCK | CR_FLOCK)) {
    printf_r("CH32V4x7Flash: unlock failed, CTLR = 0x%08x\n", ctlr);
    error = true;
  }
}

//------------------------------------------------------------------------------

void CH32V4x7Flash::erase(uint32_t addr, uint32_t mode_bit) {
  unlock_flash();
  if (!wait_statr_clear(SR_BSY, TIMEOUT_SECTOR_US)) return;

  uint32_t ctlr = read_ctlr();
  write_ctlr(ctlr | mode_bit);
  if (mode_bit != CR_MER) rvd->set_mem_u32(ADDR_FLASH_ADDR, FLASH_PROG_BASE | addr);
  write_ctlr(ctlr | mode_bit | CR_STRT);

  wait_statr_clear(SR_BSY, mode_bit == CR_MER ? TIMEOUT_CHIP_US : TIMEOUT_SECTOR_US);
  write_ctlr(read_ctlr() & ~(mode_bit | CR_STRT));

  uint32_t statr = rvd->get_mem_u32(ADDR_FLASH_STATR);
  if (statr & SR_WRPRTERR) {
    printf_r("CH32V4x7Flash: write-protect error erasing 0x%08x\n", addr);
    error = true;
  }
  rvd->set_mem_u32(ADDR_FLASH_STATR, SR_EOP | SR_WRPRTERR);  // write 1 to clear
}

void CH32V4x7Flash::wipe_sector(uint32_t addr) {
  if (addr % SECTOR_SIZE) {
    printf_r("CH32V4x7Flash: wipe_sector(0x%08x) not 4 KB aligned\n", addr);
    error = true;
    return;
  }
  erase(addr, CR_PER);
}

void CH32V4x7Flash::wipe_page(uint32_t addr) {
  if (addr % SECTOR_SIZE == 0) {
    wipe_sector(addr);
  } else {
    printf_r("CH32V4x7Flash: CH32V4x7 cannot erase a 256-byte page (0x%08x); erase 4 KB sectors\n", addr);
    error = true;
  }
}

void CH32V4x7Flash::wipe_chip() {
  erase(0, CR_MER);
}

//------------------------------------------------------------------------------

void CH32V4x7Flash::write_page(uint32_t page_addr, const uint32_t* words) {
  uint32_t ctlr = read_ctlr();
  write_ctlr(ctlr | CR_PAGE_PG);
  if (!wait_statr_clear(SR_BSY, TIMEOUT_PAGE_US)) return;
  if (!wait_statr_clear(SR_WRBSY, TIMEOUT_WORD_US)) return;

  uint32_t dst = FLASH_PROG_BASE | page_addr;
  for (int i = 0; i < PAGE_WORDS; i++) {
    rvd->set_mem_u32(dst + 4 * i, words[i]);
    if (!wait_statr_clear(SR_WRBSY, TIMEOUT_WORD_US)) return;
  }

  write_ctlr(ctlr | CR_PAGE_PG | CR_PG_STRT);
  wait_statr_clear(SR_BSY, TIMEOUT_PAGE_US);
  write_ctlr(read_ctlr() & ~(CR_PAGE_PG | CR_PG_STRT));

  uint32_t statr = rvd->get_mem_u32(ADDR_FLASH_STATR);
  if (statr & SR_WRPRTERR) {
    printf_r("CH32V4x7Flash: write-protect error programming 0x%08x\n", page_addr);
    error = true;
  }
  rvd->set_mem_u32(ADDR_FLASH_STATR, SR_EOP | SR_WRPRTERR);
}

void CH32V4x7Flash::write_flash(uint32_t dst_addr, void* blob, int size) {
  if ((dst_addr % PAGE_SIZE) || (size % 4) || size <= 0) {
    printf_r("CH32V4x7Flash: write_flash(0x%08x, %d) needs a 256-byte aligned address and a size that is a multiple of 4\n",
             dst_addr, size);
    error = true;
    return;
  }
  unlock_flash();

  uint32_t page[PAGE_WORDS];
  const uint8_t* src = (const uint8_t*)blob;
  for (int offset = 0; offset < size; offset += PAGE_SIZE) {
    int chunk = size - offset;
    if (chunk > PAGE_SIZE) chunk = PAGE_SIZE;
    memset(page, 0xFF, sizeof(page));
    memcpy(page, src + offset, chunk);
    write_page(dst_addr + offset, page);
    if (error) return;
  }
}

//------------------------------------------------------------------------------

bool CH32V4x7Flash::verify_flash(uint32_t dst_addr, void* blob, int size) {
  uint8_t* readback = new uint8_t[size];
  rvd->get_block_aligned(FLASH_PROG_BASE | dst_addr, readback, size);

  bool ok = memcmp(readback, blob, size) == 0;
  if (!ok) {
    for (int i = 0; i < size; i++) {
      if (readback[i] != ((uint8_t*)blob)[i]) {
        printf_r("CH32V4x7Flash: verify failed at 0x%08x: want 0x%02x, got 0x%02x\n",
                 dst_addr + i, ((uint8_t*)blob)[i], readback[i]);
        break;
      }
    }
  }
  delete[] readback;
  return ok;
}

//------------------------------------------------------------------------------

void CH32V4x7Flash::dump() {
  printf_b("FLASH_STATR"); printf(" = 0x%08x\n", (unsigned)rvd->get_mem_u32(ADDR_FLASH_STATR));
  printf_b("FLASH_CTLR");  printf(" = 0x%08x\n", (unsigned)rvd->get_mem_u32(ADDR_FLASH_CTLR));
  printf_b("FLASH_OBR");   printf(" = 0x%08x\n", (unsigned)rvd->get_mem_u32(ADDR_FLASH_OBR));
  printf_b("FLASH_WPR");   printf(" = 0x%08x\n", (unsigned)rvd->get_mem_u32(ADDR_FLASH_WPR));
}

//------------------------------------------------------------------------------
