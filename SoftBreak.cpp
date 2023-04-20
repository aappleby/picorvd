#include "SoftBreak.h"

#include <assert.h>
#include <string.h>

#include "utils.h"

static const int breakpoint_max = 256;
static const int page_size = 64;
static const uint32_t BP_EMPTY = 0xDEADBEEF;

//------------------------------------------------------------------------------

SoftBreak::SoftBreak(RVDebug* rvd, WCHFlash* flash) : rvd(rvd), flash(flash) {
  breakpoints = new uint32_t[breakpoint_max];

  // FIXME - Yes, we're creating two buffers the size of the entire target
  // flash memory. For small MCUs like the CH32V003 this is OK, larger MCUs
  // will need to do something different.

  int flash_size = flash->get_flash_size();
  flash_dirty = new uint8_t[flash_size];
  flash_clean = new uint8_t[flash_size];

  int page_count = flash->get_page_count();
  break_map = new uint8_t[page_count];
  flash_map = new uint8_t[page_count];
  dirty_map = new uint8_t[page_count];
}

//------------------------------------------------------------------------------

void SoftBreak::reset() {
  breakpoint_count = 0;
  for (int i = 0; i < breakpoint_max; i++) {
    breakpoints[i] = BP_EMPTY;
  }

  // FIXME - Do we want to grab a full copy of target flash here? It would save
  // us some bookkeeping later, at the cost of a slightly slower startup.
  memset(flash_dirty, 0, sizeof(flash_dirty));
  memset(flash_clean, 0, sizeof(flash_clean));

  memset(break_map, 0, sizeof(break_map));
  memset(flash_map, 0, sizeof(flash_map));
  memset(dirty_map, 0, sizeof(dirty_map));

  halted = rvd->get_dmstatus().ALLHALTED;
}

//------------------------------------------------------------------------------

void SoftBreak::halt() {
  if (halted) return;

  rvd->halt();
  unpatch_flash();
  halted = true;
}

//------------------------------------------------------------------------------

bool SoftBreak::resume() {
  if (!halted) return true;

  // When resuming, we always step by one instruction first.
  step();

  // If we land on a breakpoint after stepping, we do _not_ need to patch
  // flash and unhalt - we can just report that we're halted again.

  uint32_t dpc = rvd->get_dpc();
  bool on_breakpoint = false;
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] == dpc) {
      on_breakpoint = true;
      break;
    }
  }

  if (!on_breakpoint) {
    patch_flash();
    halted = false;
    rvd->resume();
    return true;
  }
  else {
    //printf("Not resuming because we immediately hit a breakpoint at 0x%08x\n", dpc);
    halted = true;
    return false;
  }
}

//------------------------------------------------------------------------------

void SoftBreak::set_dpc(uint32_t pc) { rvd->set_dpc(pc); }
void SoftBreak::step()           { rvd->step(); }
bool SoftBreak::is_halted()      { return halted; }
void SoftBreak::reset_cpu()      { rvd->reset_cpu(); }

//------------------------------------------------------------------------------

int SoftBreak::set_breakpoint(uint32_t addr, int size) {
  CHECK(halted);

  if (size != 2 && size != 4) {
    printf("SoftBreak::set_breakpoint - Bad breakpoint size %d\n", size);
    return -1;
  }
  if (addr >= 0x4000 - size) {
    printf("SoftBreak::set_breakpoint - Address 0x%08x invalid\n", addr);
    return -1;
  }
  if (addr & 1) {
    printf("SoftBreak::set_breakpoint - Address 0x%08x invalid\n", addr);
    return -1;
  }

  int bp_index = -1;
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] == BP_EMPTY && bp_index == -1) {
      bp_index = i;
    }
    if (breakpoints[i] == addr) {
      printf("Breakpoint at 0x%08x already set\n", addr);
      return -1;
    }
  }

  if (bp_index == -1) {
    printf("SoftBreak::set_breakpoint() - No valid slots left\n");
    return -1;
  }

  // Store the breakpoint
  int page = addr / page_size;
  breakpoints[bp_index] = addr;
  breakpoint_count++;
  break_map[page]++;
  dirty_map[page]++;

  // If this is the first breakpoint in a page, save a clean copy of it.
  if (break_map[page] == 1) {
    int page_base = page * page_size;
    rvd->get_block_aligned(page_base, flash_clean + page_base, page_size);
    memcpy(flash_dirty + page_base, flash_clean + page_base, page_size);
  }

  // Replace breakpoint address in flash_dirty with c.ebreak
  if (size == 2) {
    auto dst = (uint16_t*)(flash_dirty + addr);
    // GDB is setting a size 2 breakpoint on a 32-bit instruction. Just ignore the checks for now.
    //CHECK((*dst & 3) != 3);
    *dst = 0x9002; // c.ebreak
  }
  else if (size == 4) {
    auto dst = (uint32_t*)(flash_dirty + addr);
    //CHECK((*dst & 3) == 3);
    *dst = 0x00100073; // ebreak
  }

  return bp_index;
}

//------------------------------------------------------------------------------

int SoftBreak::clear_breakpoint(uint32_t addr, int size) {
  CHECK(halted);

  if (size != 2 && size != 4) {
    printf("SoftBreak::set_breakpoint - Bad breakpoint size %d\n", size);
    return -1;
  }
  if (addr >= 0x4000 - size) {
    printf("SoftBreak::clear_breakpoint - Address 0x%08x invalid\n", addr);
    return -1;
  }
  if (addr & 1) {
    printf("SoftBreak::clear_breakpoint - Address 0x%08x invalid\n", addr);
    return -1;
  }

  int bp_index = -1;
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] == addr) {
      bp_index = i;
      break;
    }
  }

  if (bp_index == -1) {
    printf("SoftBreak::clear_breakpoint() - No breakpoint found at 0x%08x\n", addr);
    return -1;
  }

  // Clear the breakpoint
  int page = addr / page_size;
  CHECK(break_map[page]);

  breakpoints[bp_index] = BP_EMPTY;
  breakpoint_count--;
  break_map[page]--;
  dirty_map[page]++;

  // Restore breakpoint address in flash_dirty with original instruction
  if (size == 2) {
    auto src = (uint16_t*)(flash_clean + addr);
    auto dst = (uint16_t*)(flash_dirty + addr);
    //CHECK((*src & 3) != 3);
    //CHECK(*dst == 0x9002);
    *dst = *src;
  }
  else if (size == 4) {
    auto src = (uint32_t*)(flash_clean + addr);
    auto dst = (uint32_t*)(flash_dirty + addr);
    //CHECK((*src & 3) == 3);
    //CHECK(*dst == 0x00100073);
    *dst = *src;
  }

  return bp_index;
}

//------------------------------------------------------------------------------

void SoftBreak::clear_all_breakpoints() {
  CHECK(halted);

  for (int i = 0; i < breakpoint_max; i++) {
    breakpoints[i] = BP_EMPTY;
  }
  breakpoint_count = 0;
}

//------------------------------------------------------------------------------

bool SoftBreak::has_breakpoint(uint32_t addr) {
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] == addr) return true;
  }
  return false;
}

//------------------------------------------------------------------------------

void SoftBreak::dump() {
  printf("Breakpoints:\n");
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] != BP_EMPTY) {
      printf("%03d: 0x%08x\n", i, breakpoints[i]);
    }
  }
}

//------------------------------------------------------------------------------
// Update all pages whose breakpoint counts have changed

void SoftBreak::patch_flash() {
  CHECK(halted);

  int page_count = flash->get_page_count();
  for (int page = 0; page < page_count; page++) {
    if (!dirty_map[page]) continue;

    //printf("patching page %d to have %d breakpoints\n", page, break_map[page]);
    int page_base = page * page_size;
    flash->wipe_page(page_base);
    flash->write_flash(page_base, flash_dirty + page_base, page_size);
    flash_map[page] = break_map[page];
    dirty_map[page] = 0;
  }
}

//------------------------------------------------------------------------------
// Replace all marked pages with a clean copy

void SoftBreak::unpatch_flash() {
  CHECK(halted);

  int page_count = flash->get_page_count();
  for (int page = 0; page < page_count; page++) {
    if (!flash_map[page]) continue;

    //printf("unpatching page %d\n", page);
    int page_base = page * page_size;
    flash->wipe_page(page_base);
    flash->write_flash(page_base, flash_clean + page_base, page_size);
    flash_map[page] = 0;
    dirty_map[page] = 1;
  }
}

//------------------------------------------------------------------------------
