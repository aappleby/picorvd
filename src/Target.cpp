#include "Target.h"

#include "WCHFlash_CH32V003.h"
#include "SoftBreak.h"

//------------------------------------------------------------------------------
// CH32V003: RV32EC (QingKe V2A), 16 KB flash with 64-byte erase pages,
// 2 KB RAM. No hardware triggers: breakpoints patch flash.

static const char* memory_map_ch32v003 = R"(<?xml version="1.0"?>
<!DOCTYPE memory-map PUBLIC "+//IDN gnu.org//DTD GDB Memory Map V1.0//EN" "http://sourceware.org/gdb/gdb-memory-map.dtd">
<memory-map>
  <memory type="flash" start="0x00000000" length="0x4000">
    <property name="blocksize">64</property>
  </memory>
  <memory type="ram" start="0x20000000" length="0x800"/>
</memory-map>
)";

const Target target_ch32v003 = {
  .name        = "CH32V003",
  .gpr_count   = 16,
  .flash_size  = 16 * 1024,
  // 125 MHz / 12 = 96 ns per PIO tick, timed for the 24 MHz HSI.
  .swio_clkdiv = 12,
  .memory_map  = memory_map_ch32v003,
  .create_flash = [](RVDebug* rvd) -> WCHFlash* {
    return new CH32V003Flash(rvd, 16 * 1024);
  },
  .create_breakpoints = [](RVDebug* rvd, WCHFlash* flash) -> Breakpoints* {
    return new SoftBreak(rvd, flash);
  },
};

//------------------------------------------------------------------------------
