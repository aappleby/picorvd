#pragma once
#include <stdint.h>

typedef int (*cb_printf)(const char* format, ...);

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

  void dump(cb_printf print) {
    print("  raw          = 0x%08x\n", raw);
    print("  TDIV         = %d\n", TDIV);
    print("  SOPN         = %d\n", SOPN);
    print("  CHECKSTA     = %d\n", CHECKSTA);
    print("  CMDEXTENSTA  = %d\n", CMDEXTENSTA);
    print("  OUTSTA       = %d\n", OUTSTA);
    print("  IOMODE       = %d\n", IOMODE);
    print("  VERSION      = %d\n", VERSION);
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

  void dump(cb_printf print) {
    print("  raw          = 0x%08x\n", raw);
    print("  TDIVCFG      = %d\n", TDIVCFG   );
    print("  SOPNCFG      = %d\n", SOPNCFG   );
    print("  CHECKEN      = %d\n", CHECKEN   );
    print("  CMDEXTEN     = %d\n", CMDEXTEN  );
    print("  OUTEN        = %d\n", OUTEN     );
    print("  IOMODECFG    = %d\n", IOMODECFG );
    print("  KEY          = 0x%x\n", KEY       );
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

  void dump(cb_printf print) {
    print("  raw          = 0x%08x\n", raw);
    print("  TDIVCFG      = %d\n", TDIVCFG   );
    print("  SOPNCFG      = %d\n", SOPNCFG   );
    print("  CHECKEN      = %d\n", CHECKEN   );
    print("  CMDEXTEN     = %d\n", CMDEXTEN  );
    print("  OUTEN        = %d\n", OUTEN     );
    print("  IOMODECFG    = %d\n", IOMODECFG );
    print("  KEY          = 0x%x\n", KEY       );
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

  void dump(cb_printf print) {
    print("  raw           = 0x%08x\n", raw);
    print("  DMACTIVE      = %d\n", DMACTIVE);
    print("  NDMRESET      = %d\n", NDMRESET);
    print("  ACKHAVERESET  = %d\n", ACKHAVERESET);
    print("  RESUMEREQ     = %d\n", RESUMEREQ);
    print("  HALTREQ       = %d\n", HALTREQ);
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

  void dump(cb_printf print) {
    print("  raw           = 0x%08x\n", raw);
    print("  VERSION       = %d\n", VERSION       );
    print("  AUTHENTICATED = %d\n", AUTHENTICATED );
    print("  ANYHALTED     = %d\n", ANYHALTED     );
    print("  ALLHALTED     = %d\n", ALLHALTED     );
    print("  ANYRUNNING    = %d\n", ANYRUNNING    );
    print("  ALLRUNNING    = %d\n", ALLRUNNING    );
    print("  ANYAVAIL      = %d\n", ANYAVAIL      );
    print("  ALLAVAIL      = %d\n", ALLAVAIL      );
    print("  ANYRESUMEACK  = %d\n", ANYRESUMEACK  );
    print("  ALLRESUMEACK  = %d\n", ALLRESUMEACK  );
    print("  ANYHAVERESET  = %d\n", ANYHAVERESET  );
    print("  ALLHAVERESET  = %d\n", ALLHAVERESET  );
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

  void dump(cb_printf print) {
    print("  raw           = 0x%08x\n", raw);
    print("  DATAADDR      = %d\n", DATAADDR  );
    print("  DATASIZE      = %d\n", DATASIZE  );
    print("  DATAACCESS    = %d\n", DATAACCESS);
    print("  PAD0          = %d\n", PAD0      );
    print("  NSCRATCH      = %d\n", NSCRATCH  );
    print("  PAD1          = %d\n", PAD1      );
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

  void dump(cb_printf print) {
    print("  raw           = 0x%08x\n", raw);
    print("  DATACOUNT     = %d\n", DATACOUNT  );
    print("  CMDER         = %d\n", CMDER      );
    print("  BUSY          = %d\n", BUSY       );
    print("  PROGBUFSIZE   = %d\n", PROGBUFSIZE);
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

  void dump(cb_printf print) {
    print("  raw           = 0x%08x\n", raw);
    print("  REGNO         = 0x%x\n", REGNO      );
    print("  WRITE         = %d\n", WRITE      );
    print("  TRANSFER      = %d\n", TRANSFER   );
    print("  POSTEXEC      = %d\n", POSTEXEC   );
    print("  AARPOSTINC    = %d\n", AARPOSTINC );
    print("  AARSIZE       = %d\n", AARSIZE    );
    print("  CMDTYPE       = %d\n", CMDTYPE    );
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

  void dump(cb_printf print) {
    print("  raw           = 0x%08x\n", raw);
    print("  AUTOEXECDATA  = %d\n", AUTOEXECDATA);
    print("  AUTOEXECPROG  = %d\n", AUTOEXECPROG);
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

  void dump(cb_printf print) {
    print("  raw           = 0x%08x\n", raw);
    print("  HALTSUM0      = %d\n", HALTSUM0);
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

  void dump(cb_printf print) {
    print("  raw           = 0x%08x\n", raw);
    print("  PRV           = %d\n", PRV       );
    print("  STEP          = %d\n", STEP      );
    print("  CAUSE         = %d\n", CAUSE     );
    print("  STOPTIME      = %d\n", STOPTIME  );
    print("  STEPIE        = %d\n", STEPIE    );
    print("  EBREAKU       = %d\n", EBREAKU   );
    print("  EBREAKM       = %d\n", EBREAKM   );
    print("  XDEBUGVER     = %d\n", XDEBUGVER );
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

  void dump(cb_printf print) {
    print("  raw         = 0x%08x\n", raw);
    print("  IWDG_STOP   = %d\n", IWDG_STOP );
    print("  WWDG_STOP   = %d\n", WWDG_STOP );
    print("  TIM1_STOP   = %d\n", TIM1_STOP );
    print("  TIM2_STOP   = %d\n", TIM2_STOP );
  }
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

  void dump(cb_printf print) {
    print("  raw           = 0x%08x\n", raw);
    print("  BUSY        = %d\n", BUSY     );
    print("  WRPRTERR    = %d\n", WRPRTERR );
    print("  EOP         = %d\n", EOP      );
    print("  MODE        = %d\n", MODE     );
    print("  BOOT_LOCK   = %d\n", BOOT_LOCK);
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

  void dump(cb_printf print) {
    print("  raw         = 0x%08x\n", raw);
    print("  PG          = %d\n", PG      );
    print("  PER         = %d\n", PER     );
    print("  MER         = %d\n", MER     );
    print("  OBG         = %d\n", OBG     );
    print("  OBER        = %d\n", OBER    );
    print("  STRT        = %d\n", STRT    );
    print("  LOCK        = %d\n", LOCK    );
    print("  OBWRE       = %d\n", OBWRE   );
    print("  ERRIE       = %d\n", ERRIE   );
    print("  EOPIE       = %d\n", EOPIE   );
    print("  FLOCK       = %d\n", FLOCK   );
    print("  FTPG        = %d\n", FTPG    );
    print("  FTER        = %d\n", FTER    );
    print("  BUFLOAD     = %d\n", BUFLOAD );
    print("  BUFRST      = %d\n", BUFRST  );
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

  void dump(cb_printf print) {
    print("  raw         = 0x%08x\n", raw);
    print("  OBERR       = %d\n", OBERR       );
    print("  RDPRT       = %d\n", RDPRT       );
    print("  IWDG_SW     = %d\n", IWDG_SW     );
    print("  STANDBY_RST = %d\n", STANDBY_RST );
    print("  CFGRSTT     = %d\n", CFGRSTT     );
    print("  DATA0       = %d\n", DATA0       );
    print("  DATA1       = %d\n", DATA1       );
  }

};
static_assert(sizeof(Reg_FLASH_OBR) == 4);

//------------------------------------------------------------------------------
