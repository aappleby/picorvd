// Supported target chips. main.cpp uses the one named by the PICORVD_TARGET
// macro (default target_ch32v003); everything that differs between chips is
// described here.

#pragma once
#include "RVDebug.h"

struct WCHFlash;
struct Breakpoints;

//------------------------------------------------------------------------------

struct Target {
  const char* name;
  int gpr_count;                 // 16 (RV32E) or 32 (RV32I)
  int flash_size;                // bytes
  float swio_clkdiv;             // PIO clock divider for PicoSWIO
  const char* memory_map;        // GDB memory-map XML

  WCHFlash*    (*create_flash)(RVDebug* rvd);
  Breakpoints* (*create_breakpoints)(RVDebug* rvd, WCHFlash* flash);
};

extern const Target target_ch32v003;

//------------------------------------------------------------------------------
