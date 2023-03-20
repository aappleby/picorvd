#pragma once
#include <stdint.h>

int printf(const char * format, ...);

//------------------------------------------------------------------------------

const uint32_t ADDR_ESIG_FLACAP = 0x1FFFF7E0; // Flash capacity register 0xXXXX
const uint32_t ADDR_ESIG_UNIID1 = 0x1FFFF7E8; // UID register 1 0xXXXXXXXX
const uint32_t ADDR_ESIG_UNIID2 = 0x1FFFF7EC; // UID register 2 0xXXXXXXXX
const uint32_t ADDR_ESIG_UNIID3 = 0x1FFFF7F0; // UID register 3 0xXXXXXXXX

//------------------------------------------------------------------------------

const uint8_t ADDR_DATA0        = 0x04;
const uint8_t ADDR_DATA1        = 0x05;

const uint8_t ADDR_DMCONTROL    = 0x10;
const uint8_t ADDR_DMSTATUS     = 0x11;
const uint8_t ADDR_HARTINFO     = 0x12;
const uint8_t ADDR_ABSTRACTCS   = 0x16;
const uint8_t ADDR_COMMAND      = 0x17;
const uint8_t ADDR_AUTOCMD      = 0x18;

const uint8_t ADDR_BUF0         = 0x20;
const uint8_t ADDR_BUF1         = 0x21;
const uint8_t ADDR_BUF2         = 0x22;
const uint8_t ADDR_BUF3         = 0x23;
const uint8_t ADDR_BUF4         = 0x24;
const uint8_t ADDR_BUF5         = 0x25;
const uint8_t ADDR_BUF6         = 0x26;
const uint8_t ADDR_BUF7         = 0x27;

const uint8_t ADDR_HALTSUM0     = 0x40;

// DCSR and DPC are mentioned in the debug pdf but don't appear to be present

const uint8_t ADDR_CPBR         = 0x7C;
const uint8_t ADDR_CFGR         = 0x7D;
const uint8_t ADDR_SHDWCFGR     = 0x7E;
const uint8_t ADDR_PART         = 0x7F; // not in doc but appears to be part info

//------------------------------------------------------------------------------

const uint32_t ADDR_FLASH_ACTLR = 0x40022000;
const uint32_t ADDR_FLASH_KEYR = 0x40022004;
const uint32_t ADDR_FLASH_OBKEYR = 0x40022008;
const uint32_t ADDR_FLASH_STATR = 0x4002200C;
const uint32_t ADDR_FLASH_CTLR = 0x40022010;
const uint32_t ADDR_FLASH_ADDR = 0x40022014;
const uint32_t ADDR_FLASH_OBR = 0x4002201C;
const uint32_t ADDR_FLASH_WPR = 0x40022020;
const uint32_t ADDR_FLASH_MKEYR = 0x40022024;
const uint32_t ADDR_FLASH_BKEYR = 0x40022028;

//------------------------------------------------------------------------------

struct Reg_CPBR {
  union {
    struct {
      uint32_t TDIV        : 2;
      uint32_t PAD0        : 2;
      uint32_t SOPN        : 2;
      uint32_t PAD1        : 2;
      uint32_t CHECKSTA    : 1;
      uint32_t CMDEXTENSTA : 1;
      uint32_t OUTSTA      : 1;
      uint32_t IOMODE      : 2;
      uint32_t PAD2        : 3;
      uint32_t VERSION     : 16;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw          = 0x%08x\n", raw);
    printf("  TDIV         = %d\n", TDIV);
    printf("  SOPN         = %d\n", SOPN);
    printf("  CHECKSTA     = %d\n", CHECKSTA);
    printf("  CMDEXTENSTA  = %d\n", CMDEXTENSTA);
    printf("  OUTSTA       = %d\n", OUTSTA);
    printf("  IOMODE       = %d\n", IOMODE);
    printf("  VERSION      = %d\n", VERSION);
  }
};
static_assert(sizeof(Reg_CPBR) == 4);

//------------------------------------------------------------------------------

struct Reg_CFGR {
  union {
    struct {
      uint32_t TDIVCFG   : 2;
      uint32_t PAD0      : 2;
      uint32_t SOPNCFG   : 2;
      uint32_t PAD1      : 2;
      uint32_t CHECKEN   : 1;
      uint32_t CMDEXTEN  : 1;
      uint32_t OUTEN     : 1;
      uint32_t IOMODECFG : 2;
      uint32_t PAD2      : 3;
      uint32_t KEY       : 16;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw          = 0x%08x\n", raw);
    printf("  TDIVCFG      = %d\n", TDIVCFG   );
    printf("  SOPNCFG      = %d\n", SOPNCFG   );
    printf("  CHECKEN      = %d\n", CHECKEN   );
    printf("  CMDEXTEN     = %d\n", CMDEXTEN  );
    printf("  OUTEN        = %d\n", OUTEN     );
    printf("  IOMODECFG    = %d\n", IOMODECFG );
    printf("  KEY          = 0x%x\n", KEY       );
  }
};
static_assert(sizeof(Reg_CFGR) == 4);

//------------------------------------------------------------------------------

struct Reg_SHDWCFGR {
  union {
    struct {
      uint32_t TDIVCFG   : 2;
      uint32_t PAD0      : 2;
      uint32_t SOPNCFG   : 2;
      uint32_t PAD1      : 2;
      uint32_t CHECKEN   : 1;
      uint32_t CMDEXTEN  : 1;
      uint32_t OUTEN     : 1;
      uint32_t IOMODECFG : 2;
      uint32_t PAD2      : 3;
      uint32_t KEY       : 16;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw          = 0x%08x\n", raw);
    printf("  TDIVCFG      = %d\n", TDIVCFG   );
    printf("  SOPNCFG      = %d\n", SOPNCFG   );
    printf("  CHECKEN      = %d\n", CHECKEN   );
    printf("  CMDEXTEN     = %d\n", CMDEXTEN  );
    printf("  OUTEN        = %d\n", OUTEN     );
    printf("  IOMODECFG    = %d\n", IOMODECFG );
    printf("  KEY          = 0x%x\n", KEY       );
  }
};
static_assert(sizeof(Reg_SHDWCFGR) == 4);

//------------------------------------------------------------------------------

struct Reg_DMCONTROL {
  union {
    struct {
      uint32_t DMACTIVE     : 1;
      uint32_t NDMRESET     : 1;
      uint32_t PAD0         : 26;
      uint32_t ACKHAVERESET : 1;
      uint32_t PAD1         : 1;
      uint32_t RESUMEREQ    : 1;
      uint32_t HALTREQ      : 1;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw           = 0x%08x\n", raw);
    printf("  DMACTIVE      = %d\n", DMACTIVE);
    printf("  NDMRESET      = %d\n", NDMRESET);
    printf("  ACKHAVERESET  = %d\n", ACKHAVERESET);
    printf("  RESUMEREQ     = %d\n", RESUMEREQ);
    printf("  HALTREQ       = %d\n", HALTREQ);
  }
};
static_assert(sizeof(Reg_DMCONTROL) == 4);

//------------------------------------------------------------------------------

struct Reg_DMSTATUS {
  union {
    struct {
      uint32_t VERSION       : 4;
      uint32_t PAD0          : 3;
      uint32_t AUTHENTICATED : 1;
      uint32_t ANYHALTED     : 1;
      uint32_t ALLHALTED     : 1;
      uint32_t ANYRUNNING    : 1;
      uint32_t ALLRUNNING    : 1;
      uint32_t ANYAVAIL      : 1;
      uint32_t ALLAVAIL      : 1;
      uint32_t PAD1          : 2;
      uint32_t ANYRESUMEACK  : 1;
      uint32_t ALLRESUMEACK  : 1;
      uint32_t ANYHAVERESET  : 1;
      uint32_t ALLHAVERESET  : 1;
      uint32_t PAD2          : 12;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw           = 0x%08x\n", raw);
    printf("  VERSION       = %d\n", VERSION       );
    printf("  AUTHENTICATED = %d\n", AUTHENTICATED );
    printf("  ANYHALTED     = %d\n", ANYHALTED     );
    printf("  ALLHALTED     = %d\n", ALLHALTED     );
    printf("  ANYRUNNING    = %d\n", ANYRUNNING    );
    printf("  ALLRUNNING    = %d\n", ALLRUNNING    );
    printf("  ANYAVAIL      = %d\n", ANYAVAIL      );
    printf("  ALLAVAIL      = %d\n", ALLAVAIL      );
    printf("  ANYRESUMEACK  = %d\n", ANYRESUMEACK  );
    printf("  ALLRESUMEACK  = %d\n", ALLRESUMEACK  );
    printf("  ANYHAVERESET  = %d\n", ANYHAVERESET  );
    printf("  ALLHAVERESET  = %d\n", ALLHAVERESET  );
    /*
    printf("  ");
    if      (ALLHALTED)  printf("+halt ");
    else if (ANYHALTED)  printf("?halt ");
    else                 printf("!halt ");
    if      (ALLRUNNING) printf("+run ");
    else if (ANYRUNNING) printf("?run ");
    else                 printf("!run ");
    printf("\n");
    */
  }
};
static_assert(sizeof(Reg_DMSTATUS) == 4);

//------------------------------------------------------------------------------

struct Reg_HARTINFO {
  union {
    struct {
      uint32_t DATAADDR   : 12;
      uint32_t DATASIZE   : 4;
      uint32_t DATAACCESS : 1;
      uint32_t PAD0       : 3;
      uint32_t NSCRATCH   : 4;
      uint32_t PAD1       : 8;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw           = 0x%08x\n", raw);
    printf("  DATAADDR      = %d\n", DATAADDR  );
    printf("  DATASIZE      = %d\n", DATASIZE  );
    printf("  DATAACCESS    = %d\n", DATAACCESS);
    printf("  PAD0          = %d\n", PAD0      );
    printf("  NSCRATCH      = %d\n", NSCRATCH  );
    printf("  PAD1          = %d\n", PAD1      );
  }
};
static_assert(sizeof(Reg_HARTINFO) == 4);

//------------------------------------------------------------------------------

struct Reg_ABSTRACTCS {
  union {
    struct {
      uint32_t DATACOUNT   : 4;
      uint32_t PAD0        : 4;
      uint32_t CMDER       : 3;
      uint32_t PAD1        : 1;
      uint32_t BUSY        : 1;
      uint32_t PAD2        : 11;
      uint32_t PROGBUFSIZE : 5;
      uint32_t PAD3        : 3;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw           = 0x%08x\n", raw);
    printf("  DATACOUNT     = %d\n", DATACOUNT  );
    printf("  CMDER         = %d\n", CMDER      );
    printf("  BUSY          = %d\n", BUSY       );
    printf("  PROGBUFSIZE   = %d\n", PROGBUFSIZE);
  }
};
static_assert(sizeof(Reg_ABSTRACTCS) == 4);

//------------------------------------------------------------------------------

struct Reg_COMMAND {
  union {
    struct {
      uint32_t REGNO      : 16;
      uint32_t WRITE      : 1;
      uint32_t TRANSFER   : 1;
      uint32_t POSTEXEC   : 1;
      uint32_t AARPOSTINC : 1;
      uint32_t AARSIZE    : 3;
      uint32_t PAD0       : 1;
      uint32_t CMDTYPE    : 8;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw           = 0x%08x\n", raw);
    printf("  REGNO         = 0x%x\n", REGNO      );
    printf("  WRITE         = %d\n", WRITE      );
    printf("  TRANSFER      = %d\n", TRANSFER   );
    printf("  POSTEXEC      = %d\n", POSTEXEC   );
    printf("  AARPOSTINC    = %d\n", AARPOSTINC );
    printf("  AARSIZE       = %d\n", AARSIZE    );
    printf("  CMDTYPE       = %d\n", CMDTYPE    );
  }
};
static_assert(sizeof(Reg_COMMAND) == 4);

//------------------------------------------------------------------------------

struct Reg_AUTOCMD {
  union {
    struct {
      uint32_t AUTOEXECDATA : 12;
      uint32_t PAD0         : 4;
      uint32_t AUTOEXECPROG : 16;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw           = 0x%08x\n", raw);
    printf("  AUTOEXECDATA  = %d\n", AUTOEXECDATA);
    printf("  AUTOEXECPROG  = %d\n", AUTOEXECPROG);
  }
};
static_assert(sizeof(Reg_AUTOCMD) == 4);

//------------------------------------------------------------------------------

struct Reg_HALTSUM0 {
  union {
    struct {
      uint32_t HALTSUM0 : 1;
      uint32_t PAD0     : 31;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw           = 0x%08x\n", raw);
    printf("  HALTSUM0      = %d\n", HALTSUM0);
  }
};
static_assert(sizeof(Reg_HALTSUM0) == 4);

//------------------------------------------------------------------------------

struct Reg_DCSR {
  union {
    struct {
      uint32_t PRV       : 2;
      uint32_t STEP      : 1;
      uint32_t PAD0      : 3;
      uint32_t CAUSE     : 3;
      uint32_t STOPTIME  : 1;
      uint32_t PAD1      : 1;
      uint32_t STEPIE    : 1;
      uint32_t EBREAKU   : 1;
      uint32_t PAD2      : 2;
      uint32_t EBREAKM   : 1;
      uint32_t PAD3      : 12;
      uint32_t XDEBUGVER : 4;
    };
    uint32_t raw;
  };

  void dump() {
    printf("  raw           = 0x%08x\n", raw);
    printf("  PRV           = %d\n", PRV       );
    printf("  STEP          = %d\n", STEP      );
    printf("  CAUSE         = %d\n", CAUSE     );
    printf("  STOPTIME      = %d\n", STOPTIME  );
    printf("  STEPIE        = %d\n", STEPIE    );
    printf("  EBREAKU       = %d\n", EBREAKU   );
    printf("  EBREAKM       = %d\n", EBREAKM   );
    printf("  XDEBUGVER     = %d\n", XDEBUGVER );
  }
};
static_assert(sizeof(Reg_DCSR) == 4);

//------------------------------------------------------------------------------

struct Reg_DBGMCU_CR {
  union {
    struct {
      uint32_t IWDG_STOP : 1;
      uint32_t WWDG_STOP : 1;
      uint32_t PAD0      : 2;
      uint32_t TIM1_STOP : 1;
      uint32_t TIM2_STOP : 1;
      uint32_t PAD1      : 26;
    };
    uint32_t raw;
  };
};
static_assert(sizeof(Reg_DBGMCU_CR) == 4);

//------------------------------------------------------------------------------

struct Reg_FLASH_ACTLR {
  union {
    struct {
      uint32_t LATENCY : 2;
      uint32_t PAD0    : 30;
    };
    uint32_t raw;
  };
};
static_assert(sizeof(Reg_FLASH_ACTLR) == 4);

//------------------------------------------------------------------------------

struct Reg_FLASH_STATR {
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
    printf("  raw           = 0x%08x\n", raw);
    printf("  BUSY        = %d\n", BUSY     );
    printf("  WRPRTERR    = %d\n", WRPRTERR );
    printf("  EOP         = %d\n", EOP      );
    printf("  MODE        = %d\n", MODE     );
    printf("  BOOT_LOCK   = %d\n", BOOT_LOCK);
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
    printf("  raw         = 0x%08x\n", raw);
    printf("  PG          = %d\n", PG      );
    printf("  PER         = %d\n", PER     );
    printf("  MER         = %d\n", MER     );
    printf("  OBG         = %d\n", OBG     );
    printf("  OBER        = %d\n", OBER    );
    printf("  STRT        = %d\n", STRT    );
    printf("  LOCK        = %d\n", LOCK    );
    printf("  OBWRE       = %d\n", OBWRE   );
    printf("  ERRIE       = %d\n", ERRIE   );
    printf("  EOPIE       = %d\n", EOPIE   );
    printf("  FLOCK       = %d\n", FLOCK   );
    printf("  FTPG        = %d\n", FTPG    );
    printf("  FTER        = %d\n", FTER    );
    printf("  BUFLOAD     = %d\n", BUFLOAD );
    printf("  BUFRST      = %d\n", BUFRST  );
  }
};
static_assert(sizeof(Reg_FLASH_CTLR) == 4);

//------------------------------------------------------------------------------

struct Reg_FLASH_OBR {
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
    printf("  raw         = 0x%08x\n", raw);
    printf("  OBERR       = %d\n", OBERR       );
    printf("  RDPRT       = %d\n", RDPRT       );
    printf("  IWDG_SW     = %d\n", IWDG_SW     );
    printf("  STANDBY_RST = %d\n", STANDBY_RST );
    printf("  CFGRSTT     = %d\n", CFGRSTT     );
    printf("  DATA0       = %d\n", DATA0       );
    printf("  DATA1       = %d\n", DATA1       );
  }

};
static_assert(sizeof(Reg_FLASH_OBR) == 4);

//------------------------------------------------------------------------------

struct SLDebugger {
  void init(int swd_pin);
  void reset();

  void getp(uint8_t addr, void* out) const;
  void put(uint8_t addr, uint32_t data) const;

  uint32_t get_mem32(uint32_t addr);
  void     put_mem32(uint32_t addr, uint32_t data);

  void     get_mem32p(uint32_t addr, void* data);
  void     put_mem32p(uint32_t addr, void* data);

  void put_mem16(uint32_t addr, uint16_t data);

  void get_block32(uint32_t addr, void* data, int size_dwords);
  void put_block32(uint32_t addr, void* data, int size_dwords);

  uint32_t get_gpr(int index) const;
  void     put_gpr(int index, uint32_t gpr);

  void     get_csr(int index, void* data) const;

  Reg_DCSR get_dcsr() const { Reg_DCSR r; get_csr(0x7B0, &r); return r; }
  uint32_t get_dpc() const  { uint32_t r; get_csr(0x7B1, &r); return r; }
  uint32_t get_ds0() const  { uint32_t r; get_csr(0x7B2, &r); return r; }
  uint32_t get_ds1() const  { uint32_t r; get_csr(0x7B3, &r); return r; }

  int unlock_count = 0;
  bool is_flash_locked();
  void lock_flash();
  void unlock_flash();

  void erase_flash_page(int addr);
  void write_flash_word(int addr, uint16_t data);

  bool test_mem();

  void step();

  int halt_count = 0;

  void halt();
  void unhalt();
  void stop_watchdogs();

  void reset_cpu();

  const Reg_DMSTATUS   get_dmstatus() const;
  const Reg_ABSTRACTCS get_abstatus() const;

  int swd_pin = -1;
  uint32_t* active_prog = nullptr;

  void load_prog(uint32_t* prog);

  void save_regs();
  void load_regs();

  int  reg_count = 16;
  bool reg_saved = false;
  uint32_t regfile[32];
};

//------------------------------------------------------------------------------
