// Standalone Implementation of WCH's SWIO protocol for their CH32V003 chip.

#pragma once
#include <stdint.h>
#include <stdio.h>
#include "Bus.h"

struct Reg_CPBR;
struct Reg_CFGR;
struct Reg_SHDWCFGR;

//------------------------------------------------------------------------------

struct PicoSWIO : public Bus {
  void reset(int swd_pin);

  uint32_t get(uint32_t addr) override;
  void     put(uint32_t addr, uint32_t data) override;

  uint32_t get_partid();
  void     dump();

private:

  Reg_CPBR get_cpbr();
  Reg_CFGR get_cfgr();
  Reg_SHDWCFGR get_shdwcfgr();
  const char* addr_to_regname(uint8_t addr);

  int pin = -1;
  int cmd_count = 0;
  int pio_sm = 0;
};

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
