// API wrapper around the Risc-V Debug Module. Adds convenient register access,
// (mis)aligned memory read/write, bulk read/write, and caching of GPRs/PROG{N}
// registers to reduce traffic on the DMI bus.

// Should _not_ contain anything platform- or chip-specific.
// Memory access methods assum the target has at least 8 prog registers
// FIXME - currently assuming we have exactly 8 prog registers
// FIXME - should check actual number of prog registers at startup...

#pragma once
#include <stdint.h>
#include "Bus.h"

#define BIT_T0 (1 <<  5)
#define BIT_T1 (1 <<  6)
#define BIT_T2 (1 <<  7)
#define BIT_S0 (1 <<  8)
#define BIT_S1 (1 <<  9)
#define BIT_A0 (1 << 10)
#define BIT_A1 (1 << 11)
#define BIT_A2 (1 << 12)
#define BIT_A3 (1 << 13)
#define BIT_A4 (1 << 14)
#define BIT_A5 (1 << 15)

struct Reg_DMCONTROL;
struct Reg_DMSTATUS;
struct Reg_HARTINFO;
struct Reg_ABSTRACTCS;
struct Reg_COMMAND;
struct Reg_ABSTRACTAUTO;
struct Csr_DCSR;

//------------------------------------------------------------------------------

struct RVDebug {
  RVDebug(Bus* dmi, int reg_count);
  ~RVDebug();
  void init();
  void dump();

  //----------
  // CPU control

  bool halt();
  bool resume();
  bool step();

  bool reset();
  bool clear_err();
  bool sanity();
  bool enable_breakpoints();

  //----------
  // Run small (32 byte on CH32V003) programs from the debug program buffer

  void load_prog(const char* name, uint32_t* prog, uint32_t clobbers);
  void run_prog(bool wait_until_not_busy);
  void run_prog_slow() { run_prog(true); }
  void run_prog_fast() { run_prog(false); }

  //----------
  // Debug module register access

  uint32_t         get_data0();
  uint32_t         get_data1();
  Reg_DMCONTROL    get_dmcontrol();
  Reg_DMSTATUS     get_dmstatus();
  Reg_HARTINFO     get_hartinfo();
  Reg_ABSTRACTCS   get_abstractcs();
  Reg_COMMAND      get_command();
  Reg_ABSTRACTAUTO get_abstractauto();
  uint32_t         get_prog(int i);
  uint32_t         get_haltsum0();

  void set_data0(uint32_t d);
  void set_data1(uint32_t d);
  void set_dmcontrol(Reg_DMCONTROL r);
  void set_dmstatus(Reg_DMSTATUS r);
  void set_hartinfo(Reg_HARTINFO r);
  void set_abstractcs(Reg_ABSTRACTCS r);
  void set_command(Reg_COMMAND r);
  void set_abstractauto(Reg_ABSTRACTAUTO r);
  void set_prog(int i, uint32_t r);

  //----------
  // Debug-specific CSRs

  Csr_DCSR get_dcsr();
  uint32_t get_dpc();
  uint32_t get_dscratch0();
  uint32_t get_dscratch1();

  void set_dcsr(Csr_DCSR r);
  void set_dpc(uint32_t r);
  void set_dscratch0(uint32_t r);
  void set_dscratch1(uint32_t r);

  //----------
  // CPU register access

  int      get_gpr_count() { return reg_count; }
  uint32_t get_gpr(int index);
  void     set_gpr(int index, uint32_t gpr);

  //----------
  // CSR access

  uint32_t get_csr(int index);
  void     set_csr(int index, uint32_t csr);

  //----------
  // Memory access

  uint32_t get_mem_u32(uint32_t addr);
  uint16_t get_mem_u16(uint32_t addr);
  uint8_t  get_mem_u8 (uint32_t addr);

  void     set_mem_u32(uint32_t addr, uint32_t data);
  void     set_mem_u16(uint32_t addr, uint16_t data);
  void     set_mem_u8 (uint32_t addr, uint8_t data);

  //----------
  // Bulk memory access

  void get_block_aligned  (uint32_t addr, void* data, int size);
  void set_block_aligned  (uint32_t addr, void* data, int size);

private:

  uint32_t get_mem_u32_aligned(uint32_t addr);
  void     set_mem_u32_aligned(uint32_t addr, uint32_t data);
  void reload_regs();

  Bus* dmi;

  // Cached target state, must stay in sync
  int reg_count;

  uint32_t prog_cache[8];
  uint32_t prog_will_clobber = 0; // Bits are 1 if running the current program will clober the reg


  uint32_t reg_cache[32];
  uint32_t dirty_regs = 0;  // bits are 1 if we modified the reg on device
  uint32_t cached_regs = 0; // bits are 1 if reg_cache[i] is valid
};

//------------------------------------------------------------------------------

const int BIT_DMACTIVE     = (1 <<  0);
const int BIT_ACKHAVERESET = (1 << 28);

struct Reg_DMCONTROL {
  Reg_DMCONTROL(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }
  void dump();

  union {
    struct {
      uint32_t DMACTIVE        : 1;
      uint32_t NDMRESET        : 1;
      uint32_t CLRRESETHALTREQ : 1;
      uint32_t SETRESETHALTREQ : 1;
      uint32_t CLRKEEPALIVE    : 1;
      uint32_t SETKEEPALIVE    : 1;
      uint32_t HARTSELHI       : 10;
      uint32_t HARTSELLO       : 10;
      uint32_t HASEL           : 1;
      uint32_t ACKUNAVAIL      : 1;
      uint32_t ACKHAVERESET    : 1;
      uint32_t HARTRESET       : 1;
      uint32_t RESUMEREQ       : 1;
      uint32_t HALTREQ         : 1;
    };
    uint32_t raw;
  };
};
static_assert(sizeof(Reg_DMCONTROL) == 4);

//------------------------------------------------------------------------------

const int BIT_ALLHALTED    = (1 <<  9);
const int BIT_ALLRUNNING   = (1 << 11);
const int BIT_ALLRESUMEACK = (1 << 17);

struct Reg_DMSTATUS {
  Reg_DMSTATUS(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }
  void dump();

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
};
static_assert(sizeof(Reg_DMSTATUS) == 4);

//------------------------------------------------------------------------------

struct Reg_HARTINFO {
  Reg_HARTINFO(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }
  void dump();

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
};
static_assert(sizeof(Reg_HARTINFO) == 4);

//------------------------------------------------------------------------------

struct Reg_ABSTRACTCS {
  Reg_ABSTRACTCS(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }
  void dump();

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
};
static_assert(sizeof(Reg_ABSTRACTCS) == 4);

//------------------------------------------------------------------------------

struct Reg_COMMAND {
  Reg_COMMAND(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }
  void dump();

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
};
static_assert(sizeof(Reg_COMMAND) == 4);

//------------------------------------------------------------------------------

struct Reg_ABSTRACTAUTO {
  Reg_ABSTRACTAUTO(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }
  void dump();

  union {
    struct {
      uint32_t AUTOEXECDATA : 12;
      uint32_t PAD0         : 4;
      uint32_t AUTOEXECPROG : 16;
    };
    uint32_t raw;
  };
};
static_assert(sizeof(Reg_ABSTRACTAUTO) == 4);

//------------------------------------------------------------------------------
// FIXME what was this for?

struct Reg_DBGMCU_CR {
  Reg_DBGMCU_CR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }
  void dump();

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

struct Csr_DCSR {
  Csr_DCSR(uint32_t raw = 0) { this->raw = raw; }
  operator uint32_t() const { return raw; }
  void dump();

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
};
static_assert(sizeof(Csr_DCSR) == 4);

//------------------------------------------------------------------------------
