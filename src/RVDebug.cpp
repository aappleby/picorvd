#include "RVDebug.h"
#include <stdio.h>

#include "debug_defines.h"
#include "utils.h"

//------------------------------------------------------------------------------

RVDebug::RVDebug(Bus *dmi, int reg_count) : dmi(dmi) {
  this->reg_count = reg_count;
  init();
}

//----------------------------------------

RVDebug::~RVDebug() {
}

void RVDebug::init() {
  for (int i = 0; i < 8; i++) {
    prog_cache[i] = 0xDEADBEEF;
  }
  for (int i = 0; i < 32; i++) {
    reg_cache[i] = 0xDEADBEEF;
  }
  dirty_regs = 0;
  cached_regs = 0;
}

//------------------------------------------------------------------------------

bool RVDebug::halt() {
  LOG("RVDebug::halt()\n");

  set_dmcontrol(0x80000001);
  while(!get_dmstatus().ALLHALTED) {
    LOG("ALLHALTED not set yet\n");
  }
  set_dmcontrol(0x00000001);

  LOG("RVDebug::halt() done\n");
  return true;
}

//------------------------------------------------------------------------------

bool RVDebug::resume() {
  LOG("RVDebug::resume()\n");

  if (get_dmstatus().ALLHAVERESET) {
    LOG("RVDebug::resume() - Can't resume while in reset!\n");
    return false;
  }

  reload_regs();
  set_dmcontrol(0x40000001);

  // FIXME wat wat wat wat
  /*
  while (!get_dmstatus().ALLRESUMEACK) {
    LOG("ALLRESUMEACK not set yet\n");
  }
  */
  set_dmcontrol(0x00000001);
  cached_regs = 0;

  LOG("RVDebug::resume() done\n");
  return true;
}

//------------------------------------------------------------------------------

bool RVDebug::step() {
  LOG("RVDebug::step()\n");

  if (get_dmstatus().ALLHAVERESET) {
    LOG("RVDebug::step() - Can't step while in reset!\n");
    return false;
  }

  Csr_DCSR dcsr = get_dcsr();
  dcsr.STEP = 1;
  set_dcsr(dcsr);
  resume();
  dcsr.STEP = 0;
  set_dcsr(dcsr);

  LOG("RVDebug::step() done\n");

  return true;
}

//------------------------------------------------------------------------------

bool RVDebug::reset() {
  LOG("RVDebug::reset_cpu()\n");

  // Halt and leave halt request set
  set_dmcontrol(0x80000001);
  while (!get_dmstatus().ALLHALTED) {
    LOG("ALLHALTED not set yet 1\n");
  }

  // Set reset request
  set_dmcontrol(0x80000003);
  while (!get_dmstatus().ALLHAVERESET) {
    LOG("ALLHAVERESET not set yet\n");
  }

  // Clear reset request and hold halt request
  set_dmcontrol(0x80000001);
  // this busywait seems to be required or we hang
  while (!get_dmstatus().ALLHALTED) {
    LOG("ALLHALTED not set yet 2\n");
  }

  // Clear HAVERESET
  set_dmcontrol(0x90000001);
  while (get_dmstatus().ALLHAVERESET) {
    LOG("ALLHAVERESET not cleared yet\n");
  }

  // Clear halt request
  set_dmcontrol(0x00000001);

  // Reset cached state
  init();

  // Resetting the CPU also resets DCSR, redo it.
  enable_breakpoints();

  LOG("RVDebug::reset_cpu() done\n");

  return true;
}

//------------------------------------------------------------------------------

bool RVDebug::enable_breakpoints() {
  CHECK(get_dmstatus().ALLHALTED);

  // Turn on debug breakpoints & stop counters/timers during debug
  auto dcsr = get_dcsr();
  dcsr.EBREAKM = 1;
  dcsr.EBREAKS = 1;
  dcsr.EBREAKU = 1;
  dcsr.STEPIE = 0;
  dcsr.STOPCOUNT = 1;
  dcsr.STOPTIME = 1;
  set_dcsr(dcsr);

  return true;
}

//------------------------------------------------------------------------------

void RVDebug::load_prog(const char *name, uint32_t *prog, uint32_t clobber) {
  //LOG("RVDebug::load_prog(%s, 0x%08x, 0x%08x)\n", name, prog, clobber);

  // Upload any PROG{N} word that changed.
  for (int i = 0; i < 8; i++) {
    if (prog_cache[i] != prog[i]) {
      dmi->put(DM_PROGBUF0 + i, prog[i]);
      prog_cache[i] = prog[i];
    }
  }

  // Save any registers this program is going to clobber.
  for (int i = 0; i < reg_count; i++) {
    if (bit(clobber, i)) {
      if (!bit(cached_regs, i)) {
        if (!bit(dirty_regs, i)) {
          reg_cache[i] = get_gpr(i);
          cached_regs |= (1 << i);
        }
        else {
          CHECK(false, "RVDebug::run_prog_slow() - Reg %d is about to be clobbered, but we can't get a clean copy because it's already dirty\n");
        }
      }
    }
  }

  prog_will_clobber = clobber;

  //LOG("RVDebug::load_prog() done\n");
}

//------------------------------------------------------------------------------

void RVDebug::run_prog(bool wait_until_not_busy) {
  //LOG("RVDebug::run_prog()\n");

  // We can NOT save registers here, as doing so would clobber DATA0 which may
  // be loaded with something the program needs. >:(

  Reg_COMMAND cmd;
  cmd.POSTEXEC = 1;
  set_command(cmd);

  if (wait_until_not_busy) {
    while (get_abstractcs().BUSY) {
      //LOG("get_abstractcs().BUSY not cleared yet\n");
    }
  }
  else {
    // It takes 40 usec to do _anything_ over the debug interface, so if the
    // program is "fast" then we should _never_ see BUSY... right?
    CHECK(!get_abstractcs().BUSY);
  }

  this->dirty_regs |= prog_will_clobber;

  //LOG("RVDebug::run_prog() done\n");
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
// Getting multiple GPRs via autoexec is not supported on CH32V003 :/

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
  LOG("RVDebug::reload_regs()\n");

  for (int i = 0; i < reg_count; i++) {
    if (dirty_regs & (1 << i)) {
      if (cached_regs & (1 << i)) {
        LOG("  Reloading reg %02d\n", i);
        set_gpr(i, reg_cache[i]);
      } else {
        CHECK(false, "GPR %d is dirity and we dont' have a saved copy!\n", i);
      }
    }
  }

  dirty_regs = 0;
  LOG("RVDebug::reload_regs() done\n");
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

bool RVDebug::clear_err() {
  auto abstractcs = get_abstractcs();
  abstractcs.CMDER = 7;
  set_abstractcs(abstractcs);
  return true;
}

//------------------------------------------------------------------------------

bool RVDebug::sanity() {
  bool ok = true;

  auto dmcontrol = get_dmcontrol();
  if (dmcontrol != 0x00000001) {
    LOG_R("sanity() : DMCONTROL was not clean (was 0x%08x, expected "
           "0x00000001)\n",
           dmcontrol);
    ok = false;
  }

  auto abstractcs = get_abstractcs();
  if (abstractcs != 0x08000002) {
    LOG_R("sanity() : ABSTRACTCS was not clean (was 0x%08x, expected "
           "0x08000002)\n");
    ok = false;
  }

  auto autocmd = get_abstractauto();
  if (autocmd != 0x00000000) {
    LOG_R(
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

static uint16_t prog_get_set_u32[16] = {
  //0xe0000537,   // lui a0,0xe0000
  0x0537,
  0xe000,

  //0x0f450513,   // addi a0,a0,0xF4
  0x0513,
  0x0f45,

  0x414c,         // lw a1,4(a0)
  0x8985,         // andi a1,a1,1
  0xc591,         // beqz a1,get_u32

  //set_u32:
  0x414c,         // lw a1,4(a0)
  0x15fd,        	// addi a1,a1,-1
  0x4108,         // lw a0,0(a0)
  0xc188,         // sw a0,0(a1)
  0x9002,         // ebreak

  //get_u32:
  0x414c,         // lw a1,4(a0)
  0x418c,         // lw a1,0(a1)
  0xc10c,         // sw a1,0(a0)
  0x9002,         // ebreak
};

uint32_t RVDebug::get_mem_u32_aligned(uint32_t addr) {
  if (addr & 3) {
    LOG_R("RVDebug::get_mem_u32_aligned() - Bad address 0x%08x\n", addr);
    return 0;
  }

  load_prog("prog_get_set_u32", (uint32_t*)prog_get_set_u32, BIT_A0 | BIT_A1);
  set_data1(addr);
  run_prog_fast();

  auto result = get_data0();

  return result;
}

//------------------------------------------------------------------------------

void RVDebug::set_mem_u32_aligned(uint32_t addr, uint32_t data) {
  if (addr & 3) {
    LOG_R("RVDebug::set_mem_u32_aligned - Bad alignment 0x%08x\n", addr);
    return;
  }

  load_prog("prog_get_set_u32", (uint32_t*)prog_get_set_u32, BIT_A0 | BIT_A1);

  set_data0(data);
  set_data1(addr | 1);
  run_prog_fast();
}

//------------------------------------------------------------------------------

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

  int last_idx = (size_bytes / 4) - 1;

  run_prog_fast();
  set_abstractauto(0x00000001);

  for (int i = 0; i <= last_idx; i++) {
    if (i == last_idx) {
      set_abstractauto(0x00000000);
    }
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

  int size_dwords = size_bytes / 4;
  uint32_t *cursor = (uint32_t *)src;

  for (int i = 0; i < size_dwords; i++) {
    set_data0(*cursor++);
    if (i == 0) {
      run_prog_fast();
      set_abstractauto(0x00000001);
    }
    if (i == size_dwords - 1) {
      set_abstractauto(0x00000000);
    }
  }
}

//------------------------------------------------------------------------------

void RVDebug::dump() {
  printf("\n");
  printf_y("RVDebug::dump()\n");

  auto actual_halted = get_dmstatus().ALLHALTED;

  printf_b("prog_cache\n");
  printf("  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
         prog_cache[0], prog_cache[1], prog_cache[2], prog_cache[3],
         prog_cache[4], prog_cache[5], prog_cache[6], prog_cache[7]);
  printf_b("reg_cache\n");
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 8; x++) {
      printf("  0x%08x", reg_cache[x + y * 8]);
    }
    printf("\n");
  }
  printf_b("dirty_regs\n");
  printf("  0x%08x\n", dirty_regs);

  printf_b("cached_regs\n");
  printf("  0x%08x\n", cached_regs);

  printf_b("DM_DATA0\n");
  printf("  0x%08x\n", get_data0());

  printf_b("DM_DATA1\n");
  printf("  0x%08x\n", get_data1());

  get_dmcontrol().dump();
  get_dmstatus().dump();
  get_hartinfo().dump();
  get_abstractcs().dump();
  get_command().dump();
  get_abstractauto().dump();

  printf_b("DM_PROGBUF[N]\n");
  printf("  0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
         get_prog(0), get_prog(1), get_prog(2), get_prog(3), get_prog(4),
         get_prog(5), get_prog(6), get_prog(7));

  printf_b("DM_HALTSUM\n");
  printf("  0x%08x\n", get_haltsum0());

  if (actual_halted) {
    get_dcsr().dump();
    printf_b("CSR_DPC\n");
    printf("  0x%08x\n", get_dpc());
    printf_b("CSR_DSCRATCH0\n");
    printf("  0x%08x\n", get_dscratch0());
    printf_b("CSR_DSCRATCH1\n");
    printf("  0x%08x\n", get_dscratch1());
  }
  else {
    printf("<can't display debug CSRs while target is running>\n");
  }

  printf("\n");
}

//------------------------------------------------------------------------------

void Reg_DMCONTROL::dump() {
  printf_b("DM_DMCONTROL\n");
  printf("  0x%08x\n", raw);
  printf("  DMACTIVE:%d  NDMRESET:%d\n",
            DMACTIVE,    NDMRESET);
  printf("  CLRRESETHALTREQ:%d  SETRESETHALTREQ:%d  CLRKEEPALIVE:%d  SETKEEPALIVE:%d\n",
            CLRRESETHALTREQ,    SETRESETHALTREQ,    CLRKEEPALIVE,    SETKEEPALIVE);
  printf("  HARTSELHI:%d  HARTSELLO:%d  HASEL:%d  ACKUNAVAIL:%d  ACKHAVERESET:%d\n",
            HARTSELHI,    HARTSELLO,    HASEL,    ACKUNAVAIL,    ACKHAVERESET);
  printf("  HARTRESET:%d  RESUMEREQ:%d  HALTREQ:%d\n",
            HARTRESET,    RESUMEREQ,    HALTREQ);
}

//------------------------------------------------------------------------------

void Reg_DMSTATUS::dump() {
  printf_b("DM_DMSTATUS\n");
  printf("  0x%08x\n", raw);
  printf("  VERSION:%d  AUTHENTICATED:%d\n", VERSION, AUTHENTICATED);
  printf("  ANYHALTED:%d  ANYRUNNING:%d  ANYAVAIL:%d ANYRESUMEACK:%d  ANYHAVERESET:%d\n", ANYHALTED, ANYRUNNING, ANYAVAIL, ANYRESUMEACK, ANYHAVERESET);
  printf("  ALLHALTED:%d  ALLRUNNING:%d  ALLAVAIL:%d ALLRESUMEACK:%d  ALLHAVERESET:%d\n", ALLHALTED, ALLRUNNING, ALLAVAIL, ALLRESUMEACK, ALLHAVERESET);
}

//------------------------------------------------------------------------------

void Reg_HARTINFO::dump() {
  printf_b("DM_HARTINFO\n");
  printf("  0x%08x\n", raw);
  printf("  DATAADDR:%d  DATASIZE:%d  DATAACCESS:%d  NSCRATCH:%d\n",
    DATAADDR, DATASIZE, DATAACCESS, NSCRATCH);
}

//------------------------------------------------------------------------------

void Reg_ABSTRACTCS::dump() {
  printf_b("DM_ABSTRACTCS\n");
  printf("  0x%08x\n", raw);
  printf("  DATACOUNT:%d  CMDER:%d  BUSY:%d  PROGBUFSIZE:%d\n",
    DATACOUNT, CMDER, BUSY, PROGBUFSIZE);
}

//------------------------------------------------------------------------------

void Reg_COMMAND::dump() {
  printf_b("DM_COMMAND\n");
  printf("  0x%08x\n", raw);
  printf("  REGNO:%04x  WRITE:%d  TRANSFER:%d  POSTEXEC:%d  AARPOSTINC:%d  AARSIZE:%d  CMDTYPE:%d\n",
    REGNO, WRITE, TRANSFER, POSTEXEC, AARPOSTINC, AARSIZE, CMDTYPE);
}

//------------------------------------------------------------------------------

void Reg_ABSTRACTAUTO::dump() {
  printf_b("DM_ABSTRACTAUTO\n");
  printf("  0x%08x\n", raw);
  printf("  AUTOEXECDATA:%d  AUTOEXECPROG:%d\n",
    AUTOEXECDATA, AUTOEXECPROG);
}

//------------------------------------------------------------------------------

void Reg_DBGMCU_CR::dump() {
  printf_b("DM_DBGMCU_CR\n");
  printf("  0x%08x\n", raw);
  printf("  IWDG_STOP:%d  WWDG_STOP:%d  TIM1_STOP:%d  TIM2_STOP:%d\n",
    IWDG_STOP, WWDG_STOP, TIM1_STOP, TIM2_STOP);
}

//------------------------------------------------------------------------------

void Csr_DCSR::dump() {
  printf_b("DM_DCSR\n");
  printf("  0x%08x\n", raw);
  printf("  PRV:%d  STEP:%d  NMIP:%d  MPRVEN:%d  CAUSE:%d  STOPTIME:%d  STOPCOUNT:%d\n",
            PRV,    STEP,    NMIP,    MPRVEN,    CAUSE,    STOPTIME,    STOPCOUNT);
  printf("  STEPIE:%d  EBREAKU:%d  EBREAKS:%d  EBREAKM:%d  XDEBUGVER:%d\n",
            STEPIE,    EBREAKU,    EBREAKS,    EBREAKM,    XDEBUGVER);
}

//------------------------------------------------------------------------------
