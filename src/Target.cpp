#include "Target.h"

#include "WCHFlash_CH32V003.h"
#include "WCHFlash_CH32V4x7.h"
#include "SoftBreak.h"
#include "HardBreak.h"

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
  .mem_access  = RVDebug::MEM_DATA_MAPPED,
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
// CH32V407: RV32I (QingKe V4), 512 KB flash programmed in 256-byte pages
// and erased in 4 KB sectors.
//
// RAM is 136 KB or 200 KB depending on the RAM_CODE_MOD option bits; the map
// uses 136 KB, valid in both modes. The system area (CHIPID, ESIG, option
// bytes), peripherals and core-private space are listed so GDB lets you
// inspect them.
//
// The debug port resets into single-wire SWIO on PA13, like the CH32V003, but
// the debug clock is the 20 MHz HSI, so PicoSWIO's timing is scaled by 1.25.
// The line needs an external pull-up (about 1 kOhm to 3.3 V): the Pico's
// internal pull-up is too weak for the protocol.

static const char* memory_map_ch32v407 = R"(<?xml version="1.0"?>
<!DOCTYPE memory-map PUBLIC "+//IDN gnu.org//DTD GDB Memory Map V1.0//EN" "http://sourceware.org/gdb/gdb-memory-map.dtd">
<memory-map>
  <memory type="flash" start="0x00000000" length="0x80000">
    <property name="blocksize">4096</property>
  </memory>
  <memory type="ram" start="0x20000000" length="0x22000"/>
  <memory type="rom" start="0x1fff0000" length="0x10000"/>
  <memory type="ram" start="0x40000000" length="0x20000000"/>
  <memory type="ram" start="0xe0000000" length="0x20000000"/>
</memory-map>
)";

const Target target_ch32v407 = {
  .name        = "CH32V407",
  .gpr_count   = 32,
  .flash_size  = 512 * 1024,
  .mem_access  = RVDebug::MEM_ABSTRACT,
  .swio_clkdiv = 15,
  .memory_map  = memory_map_ch32v407,
  .create_flash = [](RVDebug* rvd) -> WCHFlash* {
    return new CH32V4x7Flash(rvd, 512 * 1024);
  },
  .create_breakpoints = [](RVDebug* rvd, WCHFlash* flash) -> Breakpoints* {
    return new HardBreak(rvd);
  },
};

//------------------------------------------------------------------------------
