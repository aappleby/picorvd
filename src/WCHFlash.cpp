#include "WCHFlash.h"
#include "utils.h"
#include "RVDebug.h"

#include "pico/stdlib.h"

const uint32_t ADDR_ESIG_FLACAP  = 0x1FFFF7E0; // Flash capacity register 0xXXXX
const uint32_t ADDR_ESIG_UNIID1  = 0x1FFFF7E8; // UID register 1 0xXXXXXXXX
const uint32_t ADDR_ESIG_UNIID2  = 0x1FFFF7EC; // UID register 2 0xXXXXXXXX
const uint32_t ADDR_ESIG_UNIID3  = 0x1FFFF7F0; // UID register 3 0xXXXXXXXX

const uint32_t ADDR_FLASH_ACTLR  = 0x40022000;
const uint32_t ADDR_FLASH_KEYR   = 0x40022004;
const uint32_t ADDR_FLASH_OBKEYR = 0x40022008;
const uint32_t ADDR_FLASH_STATR  = 0x4002200C;
const uint32_t ADDR_FLASH_CTLR   = 0x40022010;
const uint32_t ADDR_FLASH_ADDR   = 0x40022014;
const uint32_t ADDR_FLASH_OBR    = 0x4002201C;
const uint32_t ADDR_FLASH_WPR    = 0x40022020;
const uint32_t ADDR_FLASH_MKEYR  = 0x40022024;
const uint32_t ADDR_FLASH_BKEYR  = 0x40022028;

//static const int ch32v003_flash_size = 16*1024;
//static const int ch32v003_page_size  = 64;
//static const int ch32v003_page_count = ch32v003_flash_size / ch32v003_page_size;

//------------------------------------------------------------------------------

struct Reg_FLASH_ACTLR {
  Reg_FLASH_ACTLR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

  union {
    struct {
      uint32_t LATENCY : 2;
      uint32_t PAD0    : 30;
    };
    uint32_t raw;
  };

  void dump() {
    printf_b("FLASH_ACTLR");
    printf(" = 0x%08x\n", raw);
    printf("  LATENCY:%d\n",
      LATENCY);
  }
};
static_assert(sizeof(Reg_FLASH_ACTLR) == 4);

//------------------------------------------------------------------------------

struct Reg_FLASH_STATR {
  Reg_FLASH_STATR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

  union {
    struct {
      uint32_t BUSY      : 1; // True if flash busy
      uint32_t PAD0      : 3;
      uint32_t WRPRTERR  : 1; // True if the flash was written while locked
      uint32_t EOP       : 1; // True if flash finished programming
      uint32_t PAD1      : 8;
      uint32_t MODE      : 1; // Something to do with boot area flash?
      uint32_t BOOT_LOCK : 1; // True if boot flash locked
      uint32_t PAD2      : 16;
    };
    uint32_t raw;
  };

  void dump() {
    printf_b("FLASH_STATR");
    printf(" = 0x%08x\n", raw);
    printf("  BUSY:%d  WRPRTERR:%d  EOP:%d  MODE:%d  BOOT_LOCK:%d\n",
      BUSY, WRPRTERR, EOP, MODE, BOOT_LOCK);
  }
};
static_assert(sizeof(Reg_FLASH_STATR) == 4);

//------------------------------------------------------------------------------

const auto BIT_CTLR_PG      = (1 <<  0);
const auto BIT_CTLR_PER     = (1 <<  1);
const auto BIT_CTLR_MER     = (1 <<  2);
const auto BIT_CTLR_OBG     = (1 <<  4);
const auto BIT_CTLR_OBER    = (1 <<  5);
const auto BIT_CTLR_STRT    = (1 <<  6);
const auto BIT_CTLR_LOCK    = (1 <<  7);
const auto BIT_CTLR_OBWRE   = (1 <<  9);
const auto BIT_CTLR_ERRIE   = (1 << 10);
const auto BIT_CTLR_EOPIE   = (1 << 12);
const auto BIT_CTLR_FLOCK   = (1 << 15);
const auto BIT_CTLR_FTPG    = (1 << 16);
const auto BIT_CTLR_FTER    = (1 << 17);
const auto BIT_CTLR_BUFLOAD = (1 << 18);
const auto BIT_CTLR_BUFRST  = (1 << 19);

struct Reg_FLASH_CTLR {
  Reg_FLASH_CTLR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

  union {
    struct {
      uint32_t PG      : 1; // Program enable
      uint32_t PER     : 1; // Perform sector erase
      uint32_t MER     : 1; // Perform full erase
      uint32_t PAD0    : 1;
      uint32_t OBG     : 1; // Perform user-selected word programming
      uint32_t OBER    : 1; // Perform user-selected word erasure
      uint32_t STRT    : 1; // Start
      uint32_t LOCK    : 1; // Flash lock status

      uint32_t PAD1    : 1;
      uint32_t OBWRE   : 1; // User-selected word write enable
      uint32_t ERRIE   : 1; // Error status interrupt control
      uint32_t PAD2    : 1;
      uint32_t EOPIE   : 1; // EOP interrupt control
      uint32_t PAD3    : 2;
      uint32_t FLOCK   : 1; // Fast programming mode lock

      uint32_t FTPG    : 1; // Fast page programming?
      uint32_t FTER    : 1; // Fast erase
      uint32_t BUFLOAD : 1; // "Cache data into BUF"
      uint32_t BUFRST  : 1; // "BUF reset operation"
      uint32_t PAD4    : 12;
    };
    uint32_t raw;
  };

  void dump() {
    printf_b("FLASH_CTLR");
    printf(" = 0x%08x\n", raw);
    printf("  PG:%d  PER:%d  MER:%d  OBG:%d  OBER:%d  STRT:%d  LOCK:%d\n",
      PG, PER, MER, OBG, OBER, STRT, LOCK);
    printf("  OBWRE:%d  ERRIE:%d  EOPIE:%d  FLOCK:%d  FTPG:%d  FTER:%d  BUFLOAD:%d  BUFRST:%d\n",
      OBWRE, ERRIE, EOPIE, FLOCK, FTPG, FTER, BUFLOAD, BUFRST);
  }
};
static_assert(sizeof(Reg_FLASH_CTLR) == 4);

//------------------------------------------------------------------------------

struct Reg_FLASH_OBR {
  Reg_FLASH_OBR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

  union {
    struct {
      uint32_t OBERR       : 1; // Unlock OB error
      uint32_t RDPRT       : 1; // Flash read protect flag
      uint32_t IWDG_SW     : 1; // Independent watchdog enable
      uint32_t PAD0        : 1;
      uint32_t STANDBY_RST : 1; // "System reset control in Standby mode"
      uint32_t CFGRSTT     : 2; // "Configuration word reset delay time"
      uint32_t PAD1        : 3;
      uint32_t DATA0       : 8; // "Data byte 0"
      uint32_t DATA1       : 8; // "Data byte 1"
      uint32_t PAD2        : 6;
    };
    uint32_t raw;
  };

  void dump() {
    printf_b("FLASH_OBR");
    printf(" = 0x%08x\n", raw);
    printf("  OBERR:%d  RDPRT:%d  IWDG_SW:%d  STANDBY_RST:%d  CFGRSTT:%d  DATA0:%d  DATA1:%d\n",
      OBERR, RDPRT, IWDG_SW, STANDBY_RST, CFGRSTT, DATA0, DATA1);
  }

};
static_assert(sizeof(Reg_FLASH_OBR) == 4);

//------------------------------------------------------------------------------

WCHFlash::WCHFlash(RVDebug* rvd, int flash_size) : rvd(rvd), flash_size(flash_size) {}

void WCHFlash::reset() {}

//------------------------------------------------------------------------------

void WCHFlash::lock_flash() {
  auto ctlr = Reg_FLASH_CTLR(rvd->get_mem_u32(ADDR_FLASH_CTLR));
  ctlr.LOCK = true;
  ctlr.FLOCK = true;
  rvd->set_mem_u32(ADDR_FLASH_CTLR, ctlr);

  CHECK(Reg_FLASH_CTLR(rvd->get_mem_u32(ADDR_FLASH_CTLR)).LOCK, "Flash did not lock!");
  CHECK(Reg_FLASH_CTLR(rvd->get_mem_u32(ADDR_FLASH_CTLR)).FLOCK, "Flash did not lock fast mode!");
}

//------------------------------------------------------------------------------

void delay_us(int us);

void WCHFlash::unlock_flash() {
  rvd->set_mem_u32(ADDR_FLASH_KEYR,  0x45670123);
  rvd->set_mem_u32(ADDR_FLASH_KEYR,  0xCDEF89AB);

  rvd->set_mem_u32(ADDR_FLASH_MKEYR, 0x45670123);
  rvd->set_mem_u32(ADDR_FLASH_MKEYR, 0xCDEF89AB);

  //CHECK(!Reg_FLASH_CTLR(rvd->get_mem_u32(ADDR_FLASH_CTLR)).LOCK, "Flash did not unlock!");
  //CHECK(!Reg_FLASH_CTLR(rvd->get_mem_u32(ADDR_FLASH_CTLR)).FLOCK, "Flash did not unlock fast mode!");
}

//------------------------------------------------------------------------------

void WCHFlash::wipe_page(uint32_t dst_addr) {
  unlock_flash();
  dst_addr |= 0x08000000;
  run_flash_command(dst_addr, BIT_CTLR_FTER, BIT_CTLR_FTER | BIT_CTLR_STRT);
}

void WCHFlash::wipe_sector(uint32_t dst_addr) {
  unlock_flash();
  dst_addr |= 0x08000000;
  run_flash_command(dst_addr, BIT_CTLR_PER, BIT_CTLR_PER | BIT_CTLR_STRT);
}

void WCHFlash::wipe_chip() {
  unlock_flash();
  uint32_t dst_addr = 0x08000000;
  run_flash_command(dst_addr, BIT_CTLR_MER, BIT_CTLR_MER | BIT_CTLR_STRT);
}

//------------------------------------------------------------------------------
// This is some tricky code that feeds data directly from the debug interface
// to the flash page programming buffer, triggering a page write every time
// the buffer fills. This avoids needing an on-chip buffer at the cost of having
// to do some assembly programming in the debug module.

// FIXME somehow the first write after debugger restart fails on the first word...

// good - 0x19e0006f
// bad  - 0x00010040

void WCHFlash::write_flash(uint32_t dst_addr, void* blob, int size) {
  LOG("WCHFlash::write_flash(0x%08x, 0x%08x, %d)\n", dst_addr, blob, size);
  unlock_flash();

  if (size % 4) LOG_R("WCHFlash::write_flash() - Bad size %d\n", size);
  int size_dwords = size / 4;

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

  rvd->set_mem_u32(ADDR_FLASH_ADDR, dst_addr);
  rvd->set_mem_u32(ADDR_FLASH_CTLR, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  rvd->load_prog("write_flash", (uint32_t*)prog_write_flash, BIT_S0 | BIT_A0 | BIT_A1 | BIT_A2 | BIT_A3 | BIT_A4 | BIT_A5);
  rvd->set_gpr(10, 0x40022000); // flash base
  rvd->set_gpr(11, 0xE00000F4); // DATA0 @ 0xE00000F4
  rvd->set_gpr(12, dst_addr);
  rvd->set_gpr(13, BIT_CTLR_FTPG | BIT_CTLR_BUFLOAD);
  rvd->set_gpr(14, BIT_CTLR_FTPG | BIT_CTLR_STRT);
  rvd->set_gpr(15, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  bool first_word = true;
  int page_count = (size_dwords + 15) / 16;

  // Start feeding dwords to prog_write_flash.

  uint32_t busy_time = 0;

  for (int page = 0; page < page_count; page++) {
    for (int dword_idx = 0; dword_idx < 16; dword_idx++) {
      int offset = (page * page_size) + (dword_idx * sizeof(uint32_t));
      uint32_t* src = (uint32_t*)((uint8_t*)blob + offset);

      // We have to write full pages only, so if we run out of source data we
      // write 0xDEADBEEF in the empty space.
      rvd->set_data0(dword_idx < size_dwords ? *src : 0xDEADBEEF);

      if (first_word) {
        // There's a chip bug here - we can't set AUTOCMD before COMMAND or
        // things break all weird

        // This run_prog _must_ include a busywait
        rvd->run_prog_slow();
        rvd->set_abstractauto(0x00000001);
        first_word = false;
      }
      else {
        // We can write flash slightly faster if we only busy-wait at the end
        // of each page, but I am wary...
        // Waiting here takes 54443 us to write 564 bytes
        //uint32_t time_a = time_us_32();
        while (rvd->get_abstractcs().BUSY) {}
        //uint32_t time_b = time_us_32();
        //busy_time += time_b - time_a;
      }
    }
    // This is the end of a page
    // Waiting here instead of the above takes 42847 us to write 564 bytes
    //uint32_t time_a = time_us_32();
    //while (rvd->get_abstractcs().BUSY) {}
    //uint32_t time_b = time_us_32();
    //busy_time += time_b - time_a;
  }

  rvd->set_abstractauto(0x00000000);
  rvd->set_mem_u32(ADDR_FLASH_CTLR, 0);

  // Write 1 to clear EOP. Not sure if we need to do this...
  auto statr = Reg_FLASH_STATR(rvd->get_mem_u32(ADDR_FLASH_STATR));
  statr.EOP = 1;
  rvd->set_mem_u32(ADDR_FLASH_STATR, statr);

  //printf("busy_time %d\n", busy_time);

  LOG("WCHFlash::write_flash() done\n");
}

//------------------------------------------------------------------------------

bool WCHFlash::verify_flash(uint32_t dst_addr, void* blob, int size) {
  LOG("WCHFlash::verify_flash(0x%08x, 0x%08x, %d)\n", dst_addr, blob, size);

  dst_addr |= 0x08000000;

  uint8_t* readback = new uint8_t[size];
  rvd->get_block_aligned(dst_addr, readback, size);

  uint8_t* data = (uint8_t*)blob;
  bool mismatch = false;
  for (int i = 0; i < size; i++) {
    if (data[i] != readback[i]) {
      LOG_R("Flash readback failed at address 0x%08x - want 0x%02x, got 0x%02x\n", dst_addr + i, data[i], readback[i]);
      mismatch = true;
    }
  }

  delete [] readback;

  LOG("WCHFlash::verify_flash() done\n");
  return !mismatch;
}

//------------------------------------------------------------------------------
// Dumps flash regs and the first 1K of flash.

/*
const uint32_t ADDR_FLASH_ACTLR  = 0x40022000;
const uint32_t ADDR_FLASH_KEYR   = 0x40022004;
const uint32_t ADDR_FLASH_OBKEYR = 0x40022008;
 const uint32_t ADDR_FLASH_STATR  = 0x4002200C;
 const uint32_t ADDR_FLASH_CTLR   = 0x40022010;
 const uint32_t ADDR_FLASH_ADDR   = 0x40022014;
 const uint32_t ADDR_FLASH_OBR    = 0x4002201C;
 const uint32_t ADDR_FLASH_WPR    = 0x40022020;
 const uint32_t ADDR_FLASH_MKEYR  = 0x40022024;
 const uint32_t ADDR_FLASH_BKEYR  = 0x40022028;
*/

void WCHFlash::dump() {

  printf_b("FLASH_ACTLR\n");
  printf("  0x%08x\n", rvd->get_mem_u32(ADDR_FLASH_ACTLR));

  printf_b("FLASH_KEYR\n");
  printf("  0x%08x\n", rvd->get_mem_u32(ADDR_FLASH_KEYR));

  printf_b("FLASH_OBKEYR\n");
  printf("  0x%08x\n", rvd->get_mem_u32(ADDR_FLASH_OBKEYR));

  Reg_FLASH_STATR(rvd->get_mem_u32(ADDR_FLASH_STATR)).dump();

  Reg_FLASH_CTLR(rvd->get_mem_u32(ADDR_FLASH_CTLR)).dump();

  printf_b("FLASH_ADDR\n");
  printf("  0x%08x\n", rvd->get_mem_u32(ADDR_FLASH_ADDR));

  Reg_FLASH_OBR(rvd->get_mem_u32(ADDR_FLASH_OBR)).dump();

  printf_b("FLASH_WPR\n");
  printf("  0x%08x\n", rvd->get_mem_u32(ADDR_FLASH_WPR));

  printf_b("FLASH_MKEYR\n");
  printf("  0x%08x\n", rvd->get_mem_u32(ADDR_FLASH_MKEYR));

  printf_b("FLASH_BKEYR\n");
  printf("  0x%08x\n", rvd->get_mem_u32(ADDR_FLASH_BKEYR));

  /*
  int lines = flash_size / 32;
  if (lines > 24) lines = 24;
  uint32_t base = 0x08000000;
  printf_b("flash dump @ [0x%08x,0x%08x]\n", base, base + (lines * 8 * 4) - 1);

  for (int y = 0; y < lines; y++) {
    uint32_t temp[8];
    rvd->get_block_aligned(base + y * 32, temp, 32);
    for (int x = 0; x < 8; x++) {
      printf("  0x%08x", temp[x]);
    }
    printf("\n");
  }
  */
}

//------------------------------------------------------------------------------

void WCHFlash::run_flash_command(uint32_t addr, uint32_t ctl1, uint32_t ctl2) {
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

  rvd->load_prog("flash_command", (uint32_t*)prog_flash_command, BIT_A0 | BIT_A1 | BIT_A2 | BIT_A3 | BIT_A5);
  rvd->set_gpr(10, 0x40022000);   // flash base
  rvd->set_gpr(11, addr);
  rvd->set_gpr(12, ctl1);
  rvd->set_gpr(13, ctl2);
  rvd->run_prog_slow();
}

//------------------------------------------------------------------------------
