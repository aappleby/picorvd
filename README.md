# PicoRVD 

This repo contains a GDB-compatible remote debug interface for the RISC-V based CH32V003 and CH32V407 chips by WinChipHead.

It allows you to program and debug these chips without needing the proprietary WCH-Link dongle or a modified copy of OpenOCD.

The app runs on a Raspberry Pi Pico and can communicate with the CH32V003 via WCH's semi-proprietary single-wire (SWIO) interface.

**This tool is very, very alpha - beware of bugs**

## Repo Structure

**src** is the main codebase. It compiles using the Pico SDK + CMake.

**src/singlewire.pio** is the Pico PIO code that generates SWIO waveforms on GP28

**example** contains a trivial blink example that builds using cnlohr's "ch32v003fun" library. You will need gcc-riscv64-unknown-elf installed to build. See build.sh/flash.sh/debug.sh for basic usage.

**test** contains just a simple tests to exercise aligned and unaligned reads/writes via the debug interface.

## Supported chips

The target is chosen at build time with the CMake option `PICORVD_TARGET` (see `src/Target.cpp`):

| `PICORVD_TARGET` | Chip | Flash | Breakpoints |
|---|---|---|---|
| `ch32v003` (default) | CH32V003 (RV32EC) | 16 KB, 64-byte pages | SoftBreak: patched into flash |
| `ch32v407` | CH32V407 (RV32I, QingKe V4) | 512 KB, 256-byte program pages, 4 KB erase sectors | HardBreak: hardware triggers |

```
cmake -B bin -DPICORVD_TARGET=ch32v407 && make -C bin
```

CH32V407 notes:
- The debug port also resets into single-wire SWIO mode, on PA13. Its debug clock is the 20 MHz HSI, so the SWIO timing is scaled by 1.25 (PIO clock divider 15 instead of 12).
- The 1 kOhm pull-up from SWIO to +3.3V is required; with only the Pico's internal pull-up the line rises too slowly and every bit reads as 0.
- Memory is accessed with standard abstract register transfers (`RVDebug::MEM_ABSTRACT`), not the CH32V003's memory-mapped DATA0/DATA1. Flash loads therefore run at about 4 KB/s.
- The GDB memory map covers 512 KB flash and 136 KB RAM (valid for both RAM_CODE_MOD settings), plus the system area (CHIPID, ESIG, option bytes), peripherals and core-private registers.
- While the core is halted the watchdogs are frozen (DBGMCU_CR), and the target is reset and held halted before a `load` erases flash.

## Usage

Connect the SWIO pin on your CH32V device (PD1 on the CH32V003, PA13 on the CH32V407) to the Pico's SWIO pin (defaults to pin GP28), connect CH32V ground to Pico ground, and add a 1Kohm pull-up resistor from SWIO to +3.3v.

After that run "gdb-multiarch {your_binary.elf}" and type "target extended-remote /dev/ttyACM0" to connect to the debugger (replace ttyACM0 with whatever port your Pico shows up as).

Most operations should be faster than the WCH-Link by virtue of doing some basic Pico-side caching and avoiding redundant debug register read/writes.

Not all GDB remote functionality is implemented, but read/write of RAM, erasing/writing flash, setting breakpoints, and stepping should all work. The target chip can be reset via "monitor reset".

## Building:

Install the prerequisites:
```
sudo apt install cmake gcc-arm-none-eabi gcc-riscv64-unknown-elf xxd
```

Then run build.sh in the repo root. CMake should auto-fetch the Pico SDK as part of the build process.

Run "upload.sh" to upload PicoRVD to your Pico if it's connected to a Pico Debug Probe, or just use the standard hold-reset-and-reboot to mount your Pico as a flash drive and then copy bin/picorvd.uf2 to it.

## Modules

PicoRVD is broken up into a couple modules that can (in principle) be reused independently:

### PicoSWIO
Implements the WCH SWIO protocol using the Pico's PIO block. Exposes a trivial get(addr)/put(addr,data) interface. Does not currently support "fast mode", but the standard mode runs at ~800kbps and should suffice.

Spec here - https://github.com/openwch/ch32v003/blob/main/RISC-V%20QingKeV2%20Microprocessor%20Debug%20Manual.pdf

### RVDebug
Exposes the various registers in the official RISC-V debug spec along with methods to read/write memory over the main bus and halt/resume/reset the CPU.

Spec here - https://github.com/riscv/riscv-debug-spec/blob/master/riscv-debug-stable.pdf 

### WCHFlash
Interface to a WCH flash controller. CH32V003Flash and CH32V4x7Flash implement it.

CH32V003Flash: methods to read/write the CH32V003's flash. Most stuff hardcoded at the moment. WCHFlash does _not_ clobber device RAM, instead it streams data directly to the flash page buffer. This means that in theory you should be able to use it to replace flash contents without needing to reset the CPU, though I haven't tested that yet.

CH32V003 reference manual here - http://www.wch-ic.com/downloads/CH32V003RM_PDF.html

### SoftBreak
The CH32V003 chip does _not_ support any hardware breakpoints. The official WCH-Link dongle simulates breakpoints by patching and unpatching flash every time it halts/resumes the processor. SoftBreak does something similar, but with optimizations to minimize the number of page updates needed. It also avoids page updates during the common 'single-step by setting breakpoints on every instruction' thing that GDB does, which makes stepping way faster.

### HardBreak
For chips with a RISC-V trigger module (QingKe V4, e.g. the CH32V407), breakpoints use hardware triggers instead of patching flash. On these chips the smallest flash erase is 4 KB, so patching would be slow and wear the flash.

### GDBServer
Communicates with the GDB host via the Pico's USB-to-serial port. Translates the GDB remote protocol into commands for RVDebug/WCHFlash/SoftBreak.

See "Appendix E" here for spec - https://sourceware.org/gdb/current/onlinedocs/gdb.pdf

### Console
A trivial serial console on UART0 (pins GP0/GP1) that implements methods for debugging the debugger itself and basic device inspection.
Connect via "minicom -b 1000000 -D /dev/ttyACM0" (replace ttyACM0 with your debug probe port) and type "help" to get a list of commands.
