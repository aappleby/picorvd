// Supported target chips. main.cpp picks one at build time with
// -DPICORVD_TARGET=<name> (CMake option PICORVD_TARGET); everything that
// differs between chips is described here.

#pragma once
#include "RVDebug.h"

struct WCHFlash;
struct Breakpoints;

//------------------------------------------------------------------------------

struct Target {
  const char* name;
  int gpr_count;                 // 16 (RV32E) or 32 (RV32I)
  int flash_size;                // bytes
  RVDebug::MemAccess mem_access; // how RVDebug reaches memory
  float swio_clkdiv;             // PIO clock divider for PicoSWIO
  const char* memory_map;        // GDB memory-map XML

  WCHFlash*    (*create_flash)(RVDebug* rvd);
  Breakpoints* (*create_breakpoints)(RVDebug* rvd, WCHFlash* flash);
};

extern const Target target_ch32v003;
extern const Target target_ch32v407;

//------------------------------------------------------------------------------
