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

struct Reg_DMCTRL {
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
static_assert(sizeof(Reg_DMCTRL) == 4);

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

struct SLDebugger {
  void init(int swd_pin);
  void reset();

  void get(uint8_t addr, void* out) const;
  void put(uint8_t addr, uint32_t data) const;

  void halt();
  void unhalt();
  void read_bus32(uint32_t addr, void* out);
  void stop_watchdogs();

  const Reg_DMSTATUS   get_dmstatus() const;
  const Reg_ABSTRACTCS get_abstatus() const;

  uint32_t get_gpr(int index) const;
  uint32_t get_csr(int index) const;
  void get_mem_block(uint32_t base, int size_bytes, void* out);

  int swd_pin = -1;
  uint32_t* active_prog = nullptr;

  void load_prog(uint32_t* prog);
};

//------------------------------------------------------------------------------

