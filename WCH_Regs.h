#pragma once
#include "debug_defines.h"

//------------------------------------------------------------------------------
// WCH-specific debug interface config registers

const uint8_t WCH_DM_CPBR     = 0x7C;
const uint8_t WCH_DM_CFGR     = 0x7D;
const uint8_t WCH_DM_SHDWCFGR = 0x7E;
const uint8_t WCH_DM_PART     = 0x7F; // not in doc but appears to be part info

//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------

struct Reg_CPBR {
  Reg_CPBR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

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
    printf("Reg_CPBR = 0x%08x\n", raw);
    printf("  TDIV:%d  SOPN:%d  CHECKSTA:%d  CMDEXTENSTA:%d  OUTSTA:%d  IOMODE:%d  VERSION:%d\n",
      TDIV, SOPN, CHECKSTA, CMDEXTENSTA, OUTSTA, IOMODE, VERSION);
  }
};
static_assert(sizeof(Reg_CPBR) == 4);

//------------------------------------------------------------------------------

struct Reg_CFGR {
  Reg_CFGR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

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
    printf("Reg_CFGR = 0x%08x\n", raw);
    printf("  TDIVCFG:%d  SOPNCFG:%d  CHECKEN:%d  CMDEXTEN:%d  OUTEN:%d  IOMODECFG:%d  KEY:0x%04x\n",
      TDIVCFG, SOPNCFG, CHECKEN, CMDEXTEN, OUTEN, IOMODECFG, KEY);
  }
};
static_assert(sizeof(Reg_CFGR) == 4);

//------------------------------------------------------------------------------

struct Reg_SHDWCFGR {
  Reg_SHDWCFGR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

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
    printf("Reg_SHDWCFGR = 0x%08x\n", raw);
    printf("  TDIVCFG:%d  SOPNCFG:%d  CHECKEN:%d  CMDEXTEN:%d  OUTEN:%d  IOMODECFG:%d  KEY:0x%04x\n",
      TDIVCFG, SOPNCFG, CHECKEN, CMDEXTEN, OUTEN, IOMODECFG, KEY);
  }
};
static_assert(sizeof(Reg_SHDWCFGR) == 4);

//------------------------------------------------------------------------------

struct Reg_DMCONTROL {
  Reg_DMCONTROL(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

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
    printf("Reg_DMCONTROL = 0x%08x\n", raw);
    printf("  DMACTIVE:%d  NDMRESET:%d  ACKHAVERESET:%d  RESUMEREQ:%d  HALTREQ:%d\n",
      DMACTIVE, NDMRESET, ACKHAVERESET, RESUMEREQ, HALTREQ);
  }
};
static_assert(sizeof(Reg_DMCONTROL) == 4);

//------------------------------------------------------------------------------

struct Reg_DMSTATUS {
  Reg_DMSTATUS(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

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
    printf("Reg_DMSTATUS = 0x%08x\n", raw);
    printf("  VERSION:%d  AUTHENTICATED:%d\n", VERSION, AUTHENTICATED);
    printf("  ANYHALTED:%d  ANYRUNNING:%d  ANYAVAIL:%d ANYRESUMEACK:%d  ANYHAVERESET:%d\n", ANYHALTED, ANYRUNNING, ANYAVAIL, ANYRESUMEACK, ANYHAVERESET);
    printf("  ALLHALTED:%d  ALLRUNNING:%d  ALLAVAIL:%d ALLRESUMEACK:%d  ALLHAVERESET:%d\n", ALLHALTED, ALLRUNNING, ALLAVAIL, ALLRESUMEACK, ALLHAVERESET);
  }
};
static_assert(sizeof(Reg_DMSTATUS) == 4);

//------------------------------------------------------------------------------

struct Reg_HARTINFO {
  Reg_HARTINFO(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

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
    printf("Reg_HARTINFO = 0x%08x\n", raw);
    printf("  DATAADDR:%d  DATASIZE:%d  DATAACCESS:%d  NSCRATCH:%d\n",
      DATAADDR, DATASIZE, DATAACCESS, NSCRATCH);
  }
};
static_assert(sizeof(Reg_HARTINFO) == 4);

//------------------------------------------------------------------------------

struct Reg_ABSTRACTCS {
  Reg_ABSTRACTCS(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

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
    printf("Reg_ABSTRACTCS = 0x%08x\n", raw);
    printf("  DATACOUNT:%d  CMDER:%d  BUSY:%d  PROGBUFSIZE:%d\n",
      DATACOUNT, CMDER, BUSY, PROGBUFSIZE);
  }
};
static_assert(sizeof(Reg_ABSTRACTCS) == 4);

//------------------------------------------------------------------------------

struct Reg_COMMAND {
  Reg_COMMAND(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

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
    printf("Reg_COMMAND = 0x%08x\n", raw);
    printf("  REGNO:%d  WRITE:%d  TRANSFER:%d  POSTEXEC:%d  AARPOSTINC:%d  AARSIZE:%d  CMDTYPE:%d\n",
      REGNO, WRITE, TRANSFER, POSTEXEC, AARPOSTINC, AARSIZE, CMDTYPE);
  }
};
static_assert(sizeof(Reg_COMMAND) == 4);

//------------------------------------------------------------------------------

struct Reg_AUTOCMD {
  Reg_AUTOCMD(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

  union {
    struct {
      uint32_t AUTOEXECDATA : 12;
      uint32_t PAD0         : 4;
      uint32_t AUTOEXECPROG : 16;
    };
    uint32_t raw;
  };

  void dump() {
    printf("Reg_AUTOCMD = 0x%08x\n", raw);
    printf("  AUTOEXECDATA:%d  AUTOEXECPROG:%d\n",
      AUTOEXECDATA, AUTOEXECPROG);
  }
};
static_assert(sizeof(Reg_AUTOCMD) == 4);

//------------------------------------------------------------------------------

struct Reg_HALTSUM0 {
  Reg_HALTSUM0(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

  union {
    struct {
      uint32_t HALTSUM0 : 1;
      uint32_t PAD0     : 31;
    };
    uint32_t raw;
  };

  void dump() {
    printf("Reg_HALTSUM0    = 0x%08x\n", raw);
    printf("  HALTSUM0      = %d\n", HALTSUM0);
  }
};
static_assert(sizeof(Reg_HALTSUM0) == 4);

//------------------------------------------------------------------------------

struct Csr_DCSR {
  Csr_DCSR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

  union {
    struct {
      uint32_t PRV       : 2;
      uint32_t STEP      : 1;
      uint32_t NMIP      : 1;
      uint32_t MPRVEN    : 1;
      uint32_t PAD0      : 1;
      uint32_t CAUSE     : 3;
      uint32_t STOPTIME  : 1;
      uint32_t STOPCOUNT : 1;
      uint32_t STEPIE    : 1;
      uint32_t EBREAKU   : 1;
      uint32_t EBREAKS   : 1;
      uint32_t PAD1      : 1;
      uint32_t EBREAKM   : 1;
      uint32_t PAD3      : 12;
      uint32_t XDEBUGVER : 4;
    };
    uint32_t raw = 0;
  };

  void dump() {
    printf("Reg_DCSR        = 0x%08x\n", raw);
    printf("  PRV:%d  STEP:%d  NMIP:%d  MPRVEN:%d  CAUSE:%d  STOPTIME:%d  STOPCOUNT:%d\n",
              PRV,    STEP,    NMIP,    MPRVEN,    CAUSE,    STOPTIME,    STOPCOUNT);
    printf("  STEPIE:%d  EBREAKU:%d  EBREAKS:%d  EBREAKM:%d  XDEBUGVER:%d\n",
              STEPIE,    EBREAKU,    EBREAKS,    EBREAKM,    XDEBUGVER);
  }
};
static_assert(sizeof(Csr_DCSR) == 4);

//------------------------------------------------------------------------------

struct Reg_DBGMCU_CR {
  Reg_DBGMCU_CR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }

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

  void dump() {
    printf("Reg_DBGMCU_CR = 0x%08x\n", raw);
    printf("  IWDG_STOP:%d  WWDG_STOP:%d  TIM1_STOP:%d  TIM2_STOP:%d\n",
      IWDG_STOP, WWDG_STOP, TIM1_STOP, TIM2_STOP);
  }
};
static_assert(sizeof(Reg_DBGMCU_CR) == 4);

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
    printf("Reg_FLASH_ACTLR = 0x%08x\n", raw);
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
    printf("Reg_FLASH_STATR = 0x%08x\n", raw);
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
    printf("Reg_FLASH_CTLR = 0x%08x\n", raw);
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
    printf("Reg_FLASH_OBR = 0x%08x\n", raw);
    printf("  OBERR:%d  RDPRT:%d  IWDG_SW:%d  STANDBY_RST:%d  CFGRSTT:%d  DATA0:%d  DATA1:%d\n",
      OBERR, RDPRT, IWDG_SW, STANDBY_RST, CFGRSTT, DATA0, DATA1);
  }

};
static_assert(sizeof(Reg_FLASH_OBR) == 4);

//------------------------------------------------------------------------------
