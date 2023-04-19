#include "SLDebugger.h"
#include "WCH_Regs.h"
#include <assert.h>
#include <string.h>

// Setting reset request clears RESUMEACK

//------------------------------------------------------------------------------

SLDebugger::SLDebugger() {
}

//------------------------------------------------------------------------------

void SLDebugger::reset_dbg(int swd_pin) {
  
  wire.reset_dbg(swd_pin);

  breakpoint_count = 0;
  for (int i = 0; i < breakpoint_max; i++) {
    breakpoints[i] = BP_EMPTY;
  }

  memset(break_map, 0, sizeof(break_map));
  memset(flash_map, 0, sizeof(flash_map));
  memset(dirty_map, 0, sizeof(dirty_map));
  memset(flash_dirty, 0, sizeof(flash_dirty));
  memset(flash_clean, 0, sizeof(flash_clean));
}

//------------------------------------------------------------------------------

void SLDebugger::halt() {
  CHECK(!wire.is_halted(), "SLDebugger::halt() - was already halted\n");

  wire.halt();
  //halt_cause = Csr_DCSR(wire.get_csr(CSR_DCSR)).CAUSE;
  unpatch_flash();
}

//------------------------------------------------------------------------------

void SLDebugger::async_halted() {
  CHECK(!wire.is_halted(), "SLDebugger::async_halted() - Was already halted\n");

  //halt_cause = Csr_DCSR(wire.get_csr(CSR_DCSR)).CAUSE;
  unpatch_flash();
}

//------------------------------------------------------------------------------

bool SLDebugger::resume() {
  CHECK(wire.is_halted(), "SLDebugger::resume() - was already resumed\n");

  // When resuming, we always step by one instruction first.
  step();

  // If we land on a breakpoint after stepping, we do _not_ need to patch
  // flash and unhalt - we can just report that we're halted again.

  uint32_t dpc = wire.get_csr(CSR_DPC);
  bool on_breakpoint = false;
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] == dpc) {
      on_breakpoint = true;
      break;
    }      
  }

  if (!on_breakpoint) {
    patch_flash();
    wire.resume();
    return true;
  }
  else {
    printf("Not resuming because we immediately hit a breakpoint at 0x%08x\n", dpc);
    return false;
  }
}

//------------------------------------------------------------------------------

void SLDebugger::step() {
  wire.step();
}

//------------------------------------------------------------------------------

void SLDebugger::reset_cpu() {
  wire.reset_cpu();
  // Resetting the CPU resets DCSR, update it again.
  patch_dcsr();
}

//------------------------------------------------------------------------------

void SLDebugger::lock_flash() {
  auto ctlr = Reg_FLASH_CTLR(wire.get_mem_u32(ADDR_FLASH_CTLR));
  ctlr.LOCK = true;
  ctlr.FLOCK = true;
  wire.set_mem_u32(ADDR_FLASH_CTLR, ctlr);

  CHECK(Reg_FLASH_CTLR(wire.get_mem_u32(ADDR_FLASH_CTLR)).LOCK);
  CHECK(Reg_FLASH_CTLR(wire.get_mem_u32(ADDR_FLASH_CTLR)).FLOCK);
}

//------------------------------------------------------------------------------

void SLDebugger::unlock_flash() {
  wire.set_mem_u32(ADDR_FLASH_KEYR,  0x45670123);
  wire.set_mem_u32(ADDR_FLASH_KEYR,  0xCDEF89AB);
  wire.set_mem_u32(ADDR_FLASH_MKEYR, 0x45670123);
  wire.set_mem_u32(ADDR_FLASH_MKEYR, 0xCDEF89AB);

  CHECK(!Reg_FLASH_CTLR(wire.get_mem_u32(ADDR_FLASH_CTLR)).LOCK);
  CHECK(!Reg_FLASH_CTLR(wire.get_mem_u32(ADDR_FLASH_CTLR)).FLOCK);
}

//------------------------------------------------------------------------------

void SLDebugger::wipe_page(uint32_t dst_addr) {
  unlock_flash();
  dst_addr |= 0x08000000;
  run_flash_command(dst_addr, BIT_CTLR_FTER, BIT_CTLR_FTER | BIT_CTLR_STRT);
}

void SLDebugger::wipe_sector(uint32_t dst_addr) {
  unlock_flash();
  dst_addr |= 0x08000000;
  run_flash_command(dst_addr, BIT_CTLR_PER, BIT_CTLR_PER | BIT_CTLR_STRT);
}

void SLDebugger::wipe_chip() {
  unlock_flash();
  uint32_t dst_addr = 0x08000000;
  run_flash_command(dst_addr, BIT_CTLR_MER, BIT_CTLR_MER | BIT_CTLR_STRT);
}

//------------------------------------------------------------------------------
// This is some tricky code that feeds data directly from the debug interface
// to the flash page programming buffer, triggering a page write every time
// the buffer fills. This avoids needing an on-chip buffer at the cost of having
// to do some assembly programming in the debug module.

void SLDebugger::write_flash(uint32_t dst_addr, void* data, int size) {
  unlock_flash();

  if (size % 4) printf("SLDebugger::write_flash() - Bad size %d\n", size);
  int size_dwords = size / 4;

  //printf("SLDebugger::write_flash 0x%08x 0x%08x %d\n", dst_addr, data, size);

  dst_addr |= 0x08000000;

  static const uint16_t prog_write_flash[16] = {
    // Copy word and trigger BUFLOAD
    0x4180, // lw      s0,0(a1)
    0xc200, // sw      s0,0(a2)
    0xc914, // sw      a3,16(a0)

    // waitloop1: Busywait for copy to complete - this seems to be required now?
    0x4540, // lw      s0,12(a0)
    0x8805, // andi    s0,s0,1
    0xfc75, // bnez    s0, <waitloop1>

    // Advance dest pointer and trigger START if we ended a page
    0x0611, // addi    a2,a2,4
    0x7413, // andi    s0,a2,63
    0x03f6, //
    0xe419, // bnez    s0, <end>
    0xc918, // sw      a4,16(a0)

    // waitloop2: Busywait for page write to complete
    0x4540, // lw      s0,12(a0)
    0x8805, // andi    s0,s0,1
    0xfc75, // bnez    s0, <waitloop2>

    // Reset buffer, don't need busywait as it'll complete before we send the
    // next dword.
    0xc91c, // sw      a5,16(a0)

    // Update page address
    0xc950, // sw      a2,20(a0)
  };

  wire.set_mem_u32(ADDR_FLASH_ADDR, dst_addr);
  wire.set_mem_u32(ADDR_FLASH_CTLR, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  wire.load_prog("write_flash", (uint32_t*)prog_write_flash, BIT_S0 | BIT_A0 | BIT_A1 | BIT_A2 | BIT_A3 | BIT_A4 | BIT_A5);
  wire.set_gpr(10, 0x40022000); // flash base
  wire.set_gpr(11, 0xE00000F4); // DATA0 @ 0xE00000F4
  wire.set_gpr(12, dst_addr);
  wire.set_gpr(13, BIT_CTLR_FTPG | BIT_CTLR_BUFLOAD);
  wire.set_gpr(14, BIT_CTLR_FTPG | BIT_CTLR_STRT);
  wire.set_gpr(15, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  bool first_word = true;
  int page_count = (size_dwords + 15) / 16;

  // Start feeding dwords to prog_write_flash.

  for (int page = 0; page < page_count; page++) {
    //printf("!");
    for (int dword_idx = 0; dword_idx < 16; dword_idx++) {
      //printf(".");
      int offset = (page * SLDebugger::page_size) + (dword_idx * sizeof(uint32_t));
      uint32_t* src = (uint32_t*)((uint8_t*)data + offset);

      wire.set_data0(dword_idx < size_dwords ? *src : 0xDEADBEEF);

      if (first_word) {
        // There's a chip bug here - we can't set AUTOCMD before COMMAND or
        // things break all weird

        wire.run_prog_slow();
        wire.set_dbg(DM_ABSTRACTAUTO, 0x00000001);
        first_word = false;
      }
      else {
        // We can write flash slightly faster if we only busy-wait at the end
        // of each page, but I am wary...
        while (Reg_ABSTRACTCS(wire.get_dbg(DM_ABSTRACTCS)).BUSY);
      }

    }
  }

  wire.set_dbg(DM_ABSTRACTAUTO, 0x00000000);
  wire.set_mem_u32(ADDR_FLASH_CTLR, 0);

  {
    // FIXME what was this for? why are we setting EOP?
    auto statr = Reg_FLASH_STATR(wire.get_mem_u32(ADDR_FLASH_STATR));
    statr.EOP = 1;
    wire.set_mem_u32(ADDR_FLASH_STATR, statr);
  }
}

//------------------------------------------------------------------------------

void SLDebugger::status() {
  printf("----------------------------------------\n");
  printf("SLDebugger::status()\n");

  wire.status();

  if (wire.is_halted()) {
    printf("----------\n");
    Reg_FLASH_STATR(wire.get_mem_u32(ADDR_FLASH_STATR)).dump();
    Reg_FLASH_CTLR(wire.get_mem_u32(ADDR_FLASH_CTLR)).dump();
    Reg_FLASH_OBR(wire.get_mem_u32(ADDR_FLASH_OBR)).dump();
    printf("ADDR_FLASH_WPR = 0x%08x\n", wire.get_mem_u32(ADDR_FLASH_WPR));

    printf("----------\n");
    Csr_DCSR(wire.get_csr(CSR_DCSR)).dump();
    printf("DPC : 0x%08x\n", wire.get_csr(CSR_DPC));
    printf("DS0 : 0x%08x\n", wire.get_csr(CSR_DSCRATCH0));
    printf("DS1 : 0x%08x\n", wire.get_csr(CSR_DSCRATCH1));

    printf("\n");
  }
  else {
    printf("----------\n");
    printf("<not halted>\n");
  }
}

//------------------------------------------------------------------------------
// Dumps all ram

void SLDebugger::dump_ram() {
  uint8_t temp[2048];

  wire.get_block_aligned(0x20000000, temp, 2048);

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 16; x++) {
      printf("0x%08x ", ((uint32_t*)temp)[x + y * 16]);
    }
    printf("\n");
  }
}

//------------------------------------------------------------------------------
// Dumps the first 1K of flash.

void SLDebugger::dump_flash() {
  Reg_FLASH_STATR(wire.get_mem_u32(ADDR_FLASH_STATR)).dump();
  Reg_FLASH_CTLR(wire.get_mem_u32(ADDR_FLASH_CTLR)).dump();
  Reg_FLASH_OBR(wire.get_mem_u32(ADDR_FLASH_OBR)).dump();
  printf("flash_wpr 0x%08x\n", wire.get_mem_u32(ADDR_FLASH_WPR));

  int lines = 64;

  uint8_t temp[1024];
  wire.get_block_aligned(0x00000000, temp, 1024);
  for (int y = 0; y < lines; y++) {
    for (int x = 0; x < 8; x++) {
      printf("0x%08x ", ((uint32_t*)temp)[x + y * 8]);
    }
    printf("\n");
  }
}

//------------------------------------------------------------------------------

void SLDebugger::run_flash_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2) {
  static const uint16_t prog_flash_command[16] = {
    0xc94c, // sw      a1,20(a0)
    0xc910, // sw      a2,16(a0)
    0xc914, // sw      a3,16(a0)

    // waitloop:
    0x455c, // lw      a5,12(a0)
    0x8b85, // andi    a5,a5,1
    0xfff5, // bnez    a5, <waitloop>

    0x2823, // sw      zero,16(a0)
    0x0005, //
    
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
    0x9002, // ebreak
  };

  wire.load_prog("flash_command", (uint32_t*)prog_flash_command, BIT_A0 | BIT_A1 | BIT_A2 | BIT_A3 | BIT_A5);
  wire.set_gpr(10, 0x40022000);   // flash base
  wire.set_gpr(11, addr);
  wire.set_gpr(12, ctl1);
  wire.set_gpr(13, ctl2);
  wire.run_prog_slow();
}

//------------------------------------------------------------------------------

int SLDebugger::set_breakpoint(uint32_t addr, int size) {
  if (size != 2 && size != 4) {
    printf("SLDebugger::set_breakpoint - Bad breakpoint size %d\n", size);
    return -1;
  }
  if (addr >= 0x4000 - size) {
    printf("SLDebugger::set_breakpoint - Address 0x%08x invalid\n", addr);
    return -1;
  }
  if (addr & 1) {
    printf("SLDebugger::set_breakpoint - Address 0x%08x invalid\n", addr);
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
    printf("SLDebugger::set_breakpoint() - No valid slots left\n");
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
    wire.get_block_aligned(page_base, flash_clean + page_base, page_size);
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

int SLDebugger::clear_breakpoint(uint32_t addr, int size) {
  if (size != 2 && size != 4) {
    printf("SLDebugger::set_breakpoint - Bad breakpoint size %d\n", size);
    return -1;
  }
  if (addr >= 0x4000 - size) {
    printf("SLDebugger::clear_breakpoint - Address 0x%08x invalid\n", addr);
    return -1;
  }
  if (addr & 1) {
    printf("SLDebugger::clear_breakpoint - Address 0x%08x invalid\n", addr);
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
    printf("SLDebugger::clear_breakpoint() - No breakpoint found at 0x%08x\n", addr);
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

void SLDebugger::clear_all_breakpoints() {
  bool was_halted = wire.is_halted();

  if (!was_halted) halt();

  for (int i = 0; i < breakpoint_max; i++) {
    breakpoints[i] = BP_EMPTY;
  }
  breakpoint_count = 0;

  if (!was_halted) resume();
}

//------------------------------------------------------------------------------

bool SLDebugger::has_breakpoint(uint32_t addr) {
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] == addr) return true;
  }
  return false;
}

//------------------------------------------------------------------------------

void SLDebugger::dump_breakpoints() {
  printf("//----------\n");
  printf("Breakpoints:\n");
  for (int i = 0; i < breakpoint_max; i++) {
    if (breakpoints[i] != BP_EMPTY) {
      printf("%03d: 0x%08x\n", i, breakpoints[i]);
    }
  }
  printf("//----------\n");
}

//------------------------------------------------------------------------------
// Update all pages whose breakpoint counts have changed

void SLDebugger::patch_flash() {
  for (int page = 0; page < page_count; page++) {
    patch_page(page);
  }
}

//------------------------------------------------------------------------------
// Replace all marked pages with a clean copy

void SLDebugger::unpatch_flash() {
  for (int page = 0; page < page_count; page++) {
    unpatch_page(page);
  }
}

//------------------------------------------------------------------------------

void SLDebugger::patch_page(int page) {
  if (!dirty_map[page]) return;

  printf("patching page %d to have %d breakpoints\n", page, break_map[page]);
  int page_base = page * page_size;
  wipe_page(page_base);
  write_flash(page_base, flash_dirty + page_base, page_size);
  flash_map[page] = break_map[page];
  dirty_map[page] = 0;
}

void SLDebugger::unpatch_page(int page) {
  if (!flash_map[page]) return;

  printf("unpatching page %d\n", page);
  int page_base = page * page_size;
  wipe_page(page_base);
  write_flash(page_base, flash_clean + page_base, page_size);
  flash_map[page] = 0;
  dirty_map[page] = 1;
}

//------------------------------------------------------------------------------
