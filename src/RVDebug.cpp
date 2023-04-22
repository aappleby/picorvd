#include "RVDebug.h"
//#include <string.h>
#include <stdio.h>

#include "debug_defines.h"
#include "utils.h"

#if 0
  // If we attach while running, resume.
  //if (!halted) {
  //  wire.resume();
  //}

  /*
  if (halted) {
    const char* halt_cause[] = {
      "<not halted>",
      "ebreak",
      "trigger",
      "halt request",
      "step",
      "halt on reset",
      "<invalid6>",
      "<invalid7>",
    };
    printf("SoftBreak::attach() - CPU is halted, cause = %s\n", halt_cause[dcsr_on_attach.CAUSE]);
  }
  else {
    printf("SoftBreak::attach() - CPU is running\n");
  }
  */
#endif

//------------------------------------------------------------------------------

RVDebug::RVDebug(Bus *dmi) : dmi(dmi) {}

//------------------------------------------------------------------------------

void RVDebug::reset() {
  // Reset cached state
  for (int i = 0; i < 8; i++) {
    prog_cache[i] = 0xDEADBEEF;
  }
  for (int i = 0; i < reg_count; i++) {
    reg_cache[i] = 0xDEADBEEF;
  }
  dirty_regs = 0;
  clean_regs = 0;

  // Halt momentarily if needed so we can enable breakpoints in DCSR
  bool was_halted = get_dmstatus().ALLHALTED;
  if (!was_halted) {
    halt();
  }
  
  // Turn on debug breakpoints & stop counters/timers during debug
  auto dcsr = get_dcsr();
  dcsr.EBREAKM = 1;
  dcsr.EBREAKS = 1;
  dcsr.EBREAKU = 1;
  dcsr.STEPIE = 0;
  dcsr.STOPCOUNT = 1;
  dcsr.STOPTIME = 1;
  set_dcsr(dcsr);

  // Resume if we were halted
  if (!was_halted) {
    resume();
  }
}

//------------------------------------------------------------------------------

uint32_t RVDebug::get_data0() { return dmi->get(DM_DATA0); }

uint32_t RVDebug::get_data1() { return dmi->get(DM_DATA1); }

Reg_DMCONTROL RVDebug::get_dmcontrol() { return dmi->get(DM_DMCONTROL); }

Reg_DMSTATUS RVDebug::get_dmstatus() { return dmi->get(DM_DMSTATUS); }

Reg_HARTINFO RVDebug::get_hartinfo() { return dmi->get(DM_HARTINFO); }

Reg_ABSTRACTCS RVDebug::get_abstractcs() { return dmi->get(DM_ABSTRACTCS); }

Reg_COMMAND RVDebug::get_command() { return dmi->get(DM_COMMAND); }

Reg_ABSTRACTAUTO RVDebug::get_abstractauto() {
  return dmi->get(DM_ABSTRACTAUTO);
}

uint32_t RVDebug::get_prog(int i) {
  return dmi->get(DM_PROGBUF0 + i);
}

uint32_t RVDebug::get_haltsum0() {
  return dmi->get(DM_HALTSUM0);
}

//------------------------------------------------------------------------------

void RVDebug::set_data0(uint32_t d) { dmi->put(DM_DATA0, d); }

void RVDebug::set_data1(uint32_t d) { dmi->put(DM_DATA1, d); }

void RVDebug::set_dmcontrol(Reg_DMCONTROL r) { dmi->put(DM_DMCONTROL, r); }

void RVDebug::set_dmstatus(Reg_DMSTATUS r) { dmi->put(DM_DMSTATUS, r); }

void RVDebug::set_hartinfo(Reg_HARTINFO r) { dmi->put(DM_HARTINFO, r); }

void RVDebug::set_abstractcs(Reg_ABSTRACTCS r) { dmi->put(DM_ABSTRACTCS, r); }

void RVDebug::set_command(Reg_COMMAND r) { dmi->put(DM_COMMAND, r); }

void RVDebug::set_abstractauto(Reg_ABSTRACTAUTO r) {
  dmi->put(DM_ABSTRACTAUTO, r);
}

//------------------------------------------------------------------------------

Csr_DCSR RVDebug::get_dcsr() { return get_csr(CSR_DCSR); }

uint32_t RVDebug::get_dpc() { return get_csr(CSR_DPC); }

uint32_t RVDebug::get_dscratch0() { return get_csr(CSR_DSCRATCH0); }

uint32_t RVDebug::get_dscratch1() { return get_csr(CSR_DSCRATCH1); }

void RVDebug::set_dcsr(Csr_DCSR r) { set_csr(CSR_DCSR, r); }

void RVDebug::set_dpc(uint32_t r) { set_csr(CSR_DPC, r); }

void RVDebug::set_dscratch0(uint32_t r) { set_csr(CSR_DSCRATCH0, r); }

void RVDebug::set_dscratch1(uint32_t r) { set_csr(CSR_DSCRATCH1, r); }

//------------------------------------------------------------------------------

void RVDebug::halt() {
  //printf("RVDebug::halt()\n");

  set_dmcontrol(0x80000001);
  while (!get_dmstatus().ALLHALTED) {
    //printf("not halted yet\n");
  }
  set_dmcontrol(0x00000001);

  //printf("RVDebug::halt() done\n");
}

//------------------------------------------------------------------------------

void RVDebug::resume() {
  //printf("RVDebug::resume()\n");

  set_dmcontrol(0x40000001);
  // We can't check ALLRUNNING because we might hit a breakpoint immediately
  while (!get_dmstatus().ALLRESUMEACK) {
    //printf("not resumed yet\n");
  }
  set_dmcontrol(0x00000001);

  clean_regs = 0;

  //printf("RVDebug::resume() done\n");
}

//------------------------------------------------------------------------------

void RVDebug::step() {
  printf("RVDebug::step()\n");

  Csr_DCSR dcsr = get_dcsr();
  dcsr.STEP = 1;
  set_dcsr(dcsr);

  set_dmcontrol(0x40000001);
  // We can't check ALLRUNNING because we might hit a breakpoint immediately
  while (!get_dmstatus().ALLRESUMEACK) {
  }
  // we might be able to skip this check?
  while (!get_dmstatus().ALLHALTED) {
  }
  set_dmcontrol(0x00000001);

  clean_regs = 0;

  dcsr.STEP = 0;
  set_dcsr(dcsr);

  printf("RVDebug::step() done\n");
}

//------------------------------------------------------------------------------

void RVDebug::reset_cpu() {
  printf("reset_cpu()");

  // Halt and leave halt request set
  set_dmcontrol(0x80000001);
  while (!get_dmstatus().ALLHALTED) {
  }

  // Set reset request
  set_dmcontrol(0x80000003);
  while (!get_dmstatus().ALLHAVERESET) {
  }

  // Clear reset request and hold halt request
  set_dmcontrol(0x80000001);
  // this busywait seems to be required or we hang
  while (!get_dmstatus().ALLHALTED) {
  }

  // Clear HAVERESET
  set_dmcontrol(0x90000001);
  while (get_dmstatus().ALLHAVERESET) {
  }

  // Clear halt request
  set_dmcontrol(0x00000001);

  // Resetting the CPU also resets DCSR, redo it.
  auto dcsr = get_dcsr();
  dcsr.EBREAKM = 1;
  dcsr.EBREAKS = 1;
  dcsr.EBREAKU = 1;
  dcsr.STEPIE = 0;
  dcsr.STOPCOUNT = 1;
  dcsr.STOPTIME = 1;
  set_dcsr(dcsr);

  // Reset cached state
  for (int i = 0; i < reg_count; i++) {
    reg_cache[i] = 0xDEADBEEF;
  }
  dirty_regs = 0;
  clean_regs = 0;
}

//------------------------------------------------------------------------------

void RVDebug::load_prog(const char *name, uint32_t *prog, uint32_t dirty_regs) {
  //printf("load_prog %s 0x%08x\n", name, dirty_regs);

  // Save any registers this program is going to clobber.
  for (int i = 0; i < reg_count; i++) {
    if (dirty_regs & (1 << i)) {
      if (clean_regs & (1 << i)) {
        // already got a clean copy
      } else {
        reg_cache[i] = get_gpr(i);
        clean_regs |= (1 << i);
      }
    }
  }

  this->dirty_regs |= dirty_regs;

  // Upload any PROG{N} word that changed.
  for (int i = 0; i < 8; i++) {
    if (prog_cache[i] != prog[i]) {
      dmi->put(DM_PROGBUF0 + i, prog[i]);
      prog_cache[i] = prog[i];
    }
  }

  //printf("load_prog %s done\n", name);
}

//------------------------------------------------------------------------------

uint32_t RVDebug::get_gpr(int index) {
  if (index == 16) {
    return get_dpc();
  }

  Reg_COMMAND cmd;
  cmd.REGNO = 0x1000 | index;
  cmd.TRANSFER = 1;
  cmd.AARSIZE = 2;
  set_command(cmd);

  return get_data0();
}

//------------------------------------------------------------------------------

void RVDebug::set_gpr(int index, uint32_t gpr) {
  if (index == 16) {
    set_dpc(gpr);
    return;
  } else {
    Reg_COMMAND cmd;
    cmd.REGNO = 0x1000 | index;
    cmd.WRITE = 1;
    cmd.TRANSFER = 1;
    cmd.AARSIZE = 2;

    set_data0(gpr);
    set_command(cmd);
  }
}

//------------------------------------------------------------------------------

void RVDebug::reload_regs() {
  for (int i = 0; i < reg_count; i++) {
    if (dirty_regs & (1 << i)) {
      if (clean_regs & (1 << i)) {
        set_gpr(i, reg_cache[i]);
      } else {
        CHECK(false, "GPR %d is dirity and we dont' have a saved copy!\n", i);
      }
    }
  }

  dirty_regs = 0;
}

//------------------------------------------------------------------------------

uint32_t RVDebug::get_csr(int index) {
  Reg_COMMAND cmd;
  cmd.REGNO = index;
  cmd.TRANSFER = 1;
  cmd.AARSIZE = 2;

  set_command(cmd);
  return get_data0();
}

//------------------------------------------------------------------------------

void RVDebug::set_csr(int index, uint32_t data) {
  Reg_COMMAND cmd;
  cmd.REGNO = index;
  cmd.WRITE = 1;
  cmd.TRANSFER = 1;
  cmd.AARSIZE = 2;

  set_data0(data);
  set_command(cmd);
}

//------------------------------------------------------------------------------
// Run a "slow" program that require busywaiting for it to complete.

void RVDebug::run_prog_slow() {
  Reg_COMMAND cmd;
  cmd.POSTEXEC = 1;
  set_command(cmd);
  while (get_abstractcs().BUSY) {}
}

//------------------------------------------------------------------------------
// Run a "fast" program that doesn't require a busywait after it's done.

void RVDebug::run_prog_fast() {
  Reg_COMMAND cmd = 0;
  cmd.POSTEXEC = 1;
  set_command(cmd);

  // It takes 40 usec to do _anything_ over the debug interface, so if the
  // program is "fast" then we should _never_ see BUSY... right?
  CHECK(!get_abstractcs().BUSY);
}

//------------------------------------------------------------------------------

void RVDebug::clear_err() { set_abstractcs(0x00030000); }

//------------------------------------------------------------------------------

bool RVDebug::sanity() {
  bool ok = true;

  auto dmcontrol = get_dmcontrol();
  if (dmcontrol != 0x00000001) {
    printf("sanity() : DMCONTROL was not clean (was 0x%08x, expected "
           "0x00000001)\n",
           dmcontrol);
    ok = false;
  }

  auto abstractcs = get_abstractcs();
  if (abstractcs != 0x08000002) {
    printf("sanity() : ABSTRACTCS was not clean (was 0x%08x, expected "
           "0x08000002)\n");
    ok = false;
  }

  auto autocmd = get_abstractauto();
  if (autocmd != 0x00000000) {
    printf(
        "sanity() : AUTOCMD was not clean (was 0x%08x, expected 0x00000000)\n");
    ok = false;
  }

  return ok;
}

//------------------------------------------------------------------------------

uint32_t RVDebug::get_mem_u32(uint32_t addr) {
  auto offset = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  auto data_lo = get_mem_u32_aligned(addr_lo);

  if (offset == 0)
    return data_lo;

  auto data_hi = get_mem_u32_aligned(addr_hi);

  return (data_lo >> (offset * 8)) | (data_hi << (32 - offset * 8));
}

//------------------------------------------------------------------------------

uint16_t RVDebug::get_mem_u16(uint32_t addr) {
  auto offset = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  uint32_t data_lo = get_mem_u32_aligned(addr_lo);

  if (offset < 3)
    return data_lo >> (offset * 8);

  uint32_t data_hi = get_mem_u32_aligned(addr_hi);

  return (data_lo >> 24) | (data_hi << 8);
}

//------------------------------------------------------------------------------

uint8_t RVDebug::get_mem_u8(uint32_t addr) {
  auto offset = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  uint32_t data_lo = get_mem_u32_aligned(addr_lo);

  return data_lo >> (offset * 8);
}

//------------------------------------------------------------------------------

void RVDebug::set_mem_u32(uint32_t addr, uint32_t data) {
  auto offset = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 4) & ~3;

  if (offset == 0) {
    set_mem_u32_aligned(addr_lo, data);
    return;
  }

  uint32_t data_lo = get_mem_u32_aligned(addr_lo);
  uint32_t data_hi = get_mem_u32_aligned(addr_hi);

  if (offset == 1) {
    data_lo &= 0x000000FF;
    data_hi &= 0xFFFFFF00;
    data_lo |= data << 8;
    data_hi |= data >> 24;
  } else if (offset == 2) {
    data_lo &= 0x0000FFFF;
    data_hi &= 0xFFFF0000;
    data_lo |= data << 16;
    data_hi |= data >> 16;
  } else if (offset == 3) {
    data_lo &= 0x00FFFFFF;
    data_hi &= 0xFF000000;
    data_lo |= data << 24;
    data_hi |= data >> 8;
  }

  set_mem_u32_aligned(addr_lo, data_lo);
  set_mem_u32_aligned(addr_hi, data_hi);
}

//------------------------------------------------------------------------------

void RVDebug::set_mem_u16(uint32_t addr, uint16_t data) {
  auto offset = addr & 3;
  auto addr_lo = (addr + 0) & ~3;
  auto addr_hi = (addr + 3) & ~3;

  uint32_t data_lo = get_mem_u32_aligned(addr_lo);

  if (offset == 0) {
    data_lo &= 0xFFFF0000;
    data_lo |= data << 0;
  } else if (offset == 1) {
    data_lo &= 0xFF0000FF;
    data_lo |= data << 8;
  } else if (offset == 2) {
    data_lo &= 0x0000FFFF;
    data_lo |= data << 16;
  } else if (offset == 3) {
    data_lo &= 0x00FFFFFF;
    data_lo |= data << 24;
  }

  set_mem_u32_aligned(addr_lo, data_lo);

  if (offset == 3) {
    uint32_t data_hi = get_mem_u32_aligned(addr_hi);
    data_hi &= 0xFFFFFF00;
    data_hi |= data >> 8;
    set_mem_u32_aligned(addr_hi, data_hi);
  }
}

//------------------------------------------------------------------------------

void RVDebug::set_mem_u8(uint32_t addr, uint8_t data) {
  auto offset = addr & 3;
  auto addr_lo = (addr + 0) & ~3;

  uint32_t data_lo = get_mem_u32_aligned(addr_lo);

  if (offset == 0) {
    data_lo = (data_lo & 0xFFFFFF00) | (data << 0);
  } else if (offset == 1) {
    data_lo = (data_lo & 0xFFFF00FF) | (data << 8);
  } else if (offset == 2) {
    data_lo = (data_lo & 0xFF00FFFF) | (data << 16);
  } else if (offset == 3) {
    data_lo = (data_lo & 0x00FFFFFF) | (data << 24);
  }

  set_mem_u32_aligned(addr_lo, data_lo);
}

//------------------------------------------------------------------------------

uint32_t RVDebug::get_mem_u32_aligned(uint32_t addr) {
  if (addr & 3) {
    printf("RVDebug::get_mem_u32_aligned() - Bad address 0x%08x\n", addr);
    return 0;
  }

  static uint32_t prog_get_mem[8] = {
      0xe0000537, // lui  a0, 0xE0000
      0x0f852583, // lw   a1, 0x0F8(a0)
      0x0005a583, // lw   a1, 0x000(a1)
      0x0eb52a23, // sw   a1, 0x0F4(a0)
      0x00100073, // ebreak
      0x00100073, // ebreak
      0x00100073, // ebreak
      0x00100073, // ebreak
  };

  load_prog("get_mem_u32", prog_get_mem, BIT_A0 | BIT_A1);
  set_data1(addr);
  run_prog_fast();

  auto result = get_data0();

  return result;
}

//------------------------------------------------------------------------------

void RVDebug::set_mem_u32_aligned(uint32_t addr, uint32_t data) {
  if (addr & 3) {
    printf("RVDebug::set_mem_u32_aligned - Bad alignment 0x%08x\n", addr);
    return;
  }

  static uint32_t prog_set_mem[8] = {
      0xe0000537, // lui  a0, 0xE0000
      0x0f852583, // lw   a1, 0x0F8(a0)
      0x0f452503, // lw   a0, 0x0F4(a0)
      0x00a5a023, // sw   a0, 0x000(a1)
      0x00100073, // ebreak
      0x00100073, // ebreak
      0x00100073, // ebreak
      0x00100073, // ebreak
  };

  load_prog("set_mem_u32", prog_set_mem, BIT_A0 | BIT_A1);

  set_data0(data);
  set_data1(addr);
  run_prog_fast();
}

//------------------------------------------------------------------------------
// FIXME we should handle invalid address ranges somehow...

void RVDebug::get_block_aligned(uint32_t addr, void *dst, int size_bytes) {
  CHECK((addr & 3) == 0, "RVDebug::get_block_aligned() bad address");
  CHECK((size_bytes & 3) == 0, "RVDebug::get_block_aligned() bad size");

  static uint32_t prog_get_block_aligned[8] = {
      0xe0000537, // lui    a0, 0xE0000
      0x0f852583, // lw     a1, 0x0F8(a0)
      0x0005a583, // lw     a1, 0x000(a1)
      0x0eb52a23, // sw     a1, 0x0F4(a0)
      0x0f852583, // lw     a1, 0x0F8(a0)
      0x00458593, // addi   a1, a1, 4
      0x0eb52c23, // sw     a1, 0x0F8(a0)
      0x00100073, // ebreak
  };

  load_prog("get_block_aligned", prog_get_block_aligned, BIT_A0 | BIT_A1);
  set_data1(addr);

  uint32_t *cursor = (uint32_t *)dst;
  for (int i = 0; i < size_bytes / 4; i++) {
    run_prog_fast();
    cursor[i] = get_data0();
  }
}

//------------------------------------------------------------------------------

void RVDebug::get_block_unaligned(uint32_t addr, void *dst, int size_bytes) {
  static uint32_t prog_get_block_unaligned[8] = {
      0xe0000537, // lui    a0, 0xE0000
      0x0f852583, // lw     a1, 0x0F8(a0)
      0x00058583, // lb     a1, 0x000(a1)
      0x0eb52a23, // sw     a1, 0x0F4(a0)
      0x0f852583, // lw     a1, 0x0F8(a0)
      0x00158593, // addi   a1, a1, 1
      0x0eb52c23, // sw     a1, 0x0F8(a0)
      0x00100073, // ebreak
  };

  load_prog("get_block_unaligned", prog_get_block_unaligned, BIT_A0 | BIT_A1);
  set_data1(addr);

  uint8_t *cursor = (uint8_t *)dst;
  for (int i = 0; i < size_bytes; i++) {
    run_prog_fast();
    cursor[i] = get_data0();
  }
}

//------------------------------------------------------------------------------

void RVDebug::set_block_aligned(uint32_t addr, void *src, int size_bytes) {
  CHECK((addr & 3) == 0);
  CHECK((size_bytes & 3) == 0);

  static uint32_t prog_set_block_aligned[8] = {
      0xe0000537, // lui    a0, 0xE0000
      0x0f852583, // lw     a1, 0x0F8(a0)
      0x0f452503, // lw     a0, 0x0F4(a0)
      0x00a5a023, // sw     a0, a1
      0x00458593, // addi   a1, a1, 4
      0xe0000537, // lui    a0, 0xE0000
      0x0eb52c23, // sw     a1, 0xF8(a0)
      0x00100073, // ebreak
  };

  load_prog("set_block_aligned", prog_set_block_aligned, BIT_A0 | BIT_A1);
  set_data1(addr);

  uint32_t *cursor = (uint32_t *)src;
  for (int i = 0; i < size_bytes / 4; i++) {
    set_data0(*cursor++);
    run_prog_fast();
  }
}

//------------------------------------------------------------------------------

void RVDebug::set_block_unaligned(uint32_t addr, void *src, int size_bytes) {
  static uint32_t prog_set_block_unaligned[8] = {
      0xe0000537, // lui    a0, 0xE0000
      0x0f852583, // lw     a1, 0x0F8(a0)
      0x0f452503, // lw     a0, 0x0F4(a0)
      0x00a58023, // sb     a0, 0x000(a1)
      0x00158593, // addi   a1, a1, 1
      0xe0000537, // lui    a0, 0xE0000
      0x0eb52c23, // sw     a1, 0x0F8(a0)
      0x00100073, // ebreak
  };

  load_prog("set_block_unaligned", prog_set_block_unaligned, BIT_A0 | BIT_A1);
  set_data1(addr);

  uint8_t *cursor = (uint8_t *)src;
  for (int i = 0; i < size_bytes; i++) {
    set_data0(*cursor++);
    run_prog_fast();
  }
}

//------------------------------------------------------------------------------
// Dumps all ram

#if 0
void RVDebug::dump_ram() {
  uint8_t temp[2048];

  get_block_aligned(0x20000000, temp, 2048);

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 16; x++) {
      printf("0x%08x ", ((uint32_t*)temp)[x + y * 16]);
    }
    printf("\n");
  }
}
#endif

//------------------------------------------------------------------------------

void RVDebug::dump() {
  auto actual_halted = get_dmstatus().ALLHALTED;

  printf("REG_DATA0 = 0x%08x\n", get_data0());
  printf("REG_DATA1 = 0x%08x\n", get_data1());
  get_dmcontrol().dump();
  get_dmstatus().dump();
  get_hartinfo().dump();
  get_abstractcs().dump();
  get_command().dump();
  get_abstractauto().dump();
  printf("PROG 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
         get_prog(0), get_prog(1), get_prog(2), get_prog(3), get_prog(4),
         get_prog(5), get_prog(6), get_prog(7));
  printf("REG_HALTSUM = 0x%08x\n", get_haltsum0());

  if (actual_halted) {
    get_dcsr().dump();
    printf("CSR_DPC = 0x%08x\n", get_dpc());
    printf("CSR_DSCRATCH0 = 0x%08x\n", get_dscratch0());
    printf("CSR_DSCRATCH1 = 0x%08x\n", get_dscratch1());
  }
  else {
    printf("<can't display debug CSRs while target is running>\n");
  }
}

//------------------------------------------------------------------------------

void Reg_DMCONTROL::dump() {
  printf("Reg_DMCONTROL = 0x%08x\n", raw);
  printf("  DMACTIVE:%d  NDMRESET:%d  ACKHAVERESET:%d  RESUMEREQ:%d  HALTREQ:%d\n",
    DMACTIVE, NDMRESET, ACKHAVERESET, RESUMEREQ, HALTREQ);
}

//------------------------------------------------------------------------------

void Reg_DMSTATUS::dump() {
  printf("Reg_DMSTATUS = 0x%08x\n", raw);
  printf("  VERSION:%d  AUTHENTICATED:%d\n", VERSION, AUTHENTICATED);
  printf("  ANYHALTED:%d  ANYRUNNING:%d  ANYAVAIL:%d ANYRESUMEACK:%d  ANYHAVERESET:%d\n", ANYHALTED, ANYRUNNING, ANYAVAIL, ANYRESUMEACK, ANYHAVERESET);
  printf("  ALLHALTED:%d  ALLRUNNING:%d  ALLAVAIL:%d ALLRESUMEACK:%d  ALLHAVERESET:%d\n", ALLHALTED, ALLRUNNING, ALLAVAIL, ALLRESUMEACK, ALLHAVERESET);
}

//------------------------------------------------------------------------------

void Reg_HARTINFO::dump() {
  printf("Reg_HARTINFO = 0x%08x\n", raw);
  printf("  DATAADDR:%d  DATASIZE:%d  DATAACCESS:%d  NSCRATCH:%d\n",
    DATAADDR, DATASIZE, DATAACCESS, NSCRATCH);
}

//------------------------------------------------------------------------------

void Reg_ABSTRACTCS::dump() {
  printf("Reg_ABSTRACTCS = 0x%08x\n", raw);
  printf("  DATACOUNT:%d  CMDER:%d  BUSY:%d  PROGBUFSIZE:%d\n",
    DATACOUNT, CMDER, BUSY, PROGBUFSIZE);
}

//------------------------------------------------------------------------------

void Reg_COMMAND::dump() {
  printf("Reg_COMMAND = 0x%08x\n", raw);
  printf("  REGNO:%d  WRITE:%d  TRANSFER:%d  POSTEXEC:%d  AARPOSTINC:%d  AARSIZE:%d  CMDTYPE:%d\n",
    REGNO, WRITE, TRANSFER, POSTEXEC, AARPOSTINC, AARSIZE, CMDTYPE);
}

//------------------------------------------------------------------------------

void Reg_ABSTRACTAUTO::dump() {
  printf("Reg_ABSTRACTAUTO = 0x%08x\n", raw);
  printf("  AUTOEXECDATA:%d  AUTOEXECPROG:%d\n",
    AUTOEXECDATA, AUTOEXECPROG);
}

//------------------------------------------------------------------------------

void Reg_DBGMCU_CR::dump() {
  printf("Reg_DBGMCU_CR = 0x%08x\n", raw);
  printf("  IWDG_STOP:%d  WWDG_STOP:%d  TIM1_STOP:%d  TIM2_STOP:%d\n",
    IWDG_STOP, WWDG_STOP, TIM1_STOP, TIM2_STOP);
}

//------------------------------------------------------------------------------

void Csr_DCSR::dump() {
  printf("Reg_DCSR = 0x%08x\n", raw);
  printf("  PRV:%d  STEP:%d  NMIP:%d  MPRVEN:%d  CAUSE:%d  STOPTIME:%d  STOPCOUNT:%d\n",
            PRV,    STEP,    NMIP,    MPRVEN,    CAUSE,    STOPTIME,    STOPCOUNT);
  printf("  STEPIE:%d  EBREAKU:%d  EBREAKS:%d  EBREAKM:%d  XDEBUGVER:%d\n",
            STEPIE,    EBREAKU,    EBREAKS,    EBREAKM,    XDEBUGVER);
}

//------------------------------------------------------------------------------
