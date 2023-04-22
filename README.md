# PicoRVD

This repo contains a GDB-compatible debugger for the RISC-V based CH32V003 series of chips by WinChipHead.

The app runs on a Raspberry Pi Pico and provides an implementation of the GDB remote stub that can communicate with the CH32V via WCH's semi-proprietary single-wire (SWIO) interface.

It allows you to program and debug a CH32V chip without needing the official WCH-Link dongle or a modified copy of OpenOCD.

## Usage

Connect pin PD0 on your CH32V device to the Pico's SWIO pin (defaults to pin GP28) and add a 1Kohm pull-up resistor from SWIO to +3.3v.

After that run "gdb-multiarch {your_binary.elf}"

```
set debug remote 1
target extended-remote /dev/ttyACM1
```

Most operations should be faster than the WCH-Link by virtue of doing some basic Pico-side caching and avoiding redundant debug register read/writes.

## Building:

Make sure PICO_SDK_PATH is set in your environment and run "build.sh" to compile.

Run "upload.sh" to upload the app to your Pico if it's connected to a Pico Debug Probe, or just use the standard hold-reset-and-reboot to mount your Pico as a flash drive and then copy bin/picorvd.uf2 to it.

NOTE - This repo is not ready for public consumption yet, I need to fix up some issues created during refactoring and merge the standalone blink example into this repo for testing.

## Modules

PicoRVD is broken up into a couple modules that can (in principle) be reused independently:

### PicoSWIO
Implements the WCH SWIO protocol using the Pico's PIO block. Exposes a trivial get(addr)/put(addr,data) interface. Does not currently support "fast mode", but the standard mode runs at ~800kbps and should suffice.

Spec here - https://github.com/openwch/ch32v003/blob/main/RISC-V%20QingKeV2%20Microprocessor%20Debug%20Manual.pdf

### RVDebug
Exposes the various registers in the official RISC-V debug spec along with methods to read/write memory over the main bus and halt/resume/reset the CPU.

Spec here - https://github.com/riscv/riscv-debug-spec/blob/master/riscv-debug-stable.pdf 

### WCHFlash
Methods to read/write the CH32V003's flash. Most stuff hardcoded at the moment. WCHFlash does _not_ clobber device RAM, instead it streams data directly to the flash page buffer. This means that in theory you should be able to use it to replace flash contents without needing to reset the CPU, though I haven't tested that yet.

CH32V003 reference manual here - http://www.wch-ic.com/downloads/CH32V003RM_PDF.html

### SoftBreak
The CH32V003 chip does _not_ support any hardware breakpoints. The official WCH-Link dongle simulates breakpoints by patching and unpatching flash every time it halts/resumes the processor. SoftBreak does something similar, but with optimizations to minimize the number of page updates needed. It also avoids page updates during the common 'single-step by setting breakpoints on every instruction' thing that GDB does, which makes stepping way faster.

### GDBServer
Communicates with the GDB host via the Pico's USB-to-serial port. Translates the GDB remote protocol into commands for RVDebug/WCHFlash/SoftBreak.

See "Appendix E" here for spec - https://sourceware.org/gdb/current/onlinedocs/gdb.pdf

### Console
A trivial serial console on UART0 (pins GP0/GP1) that implements methods for debugging the debugger itself and basic device inspection.

## TODO

SoftBreak should probably do more aggressive flash caching as GDB seems to do lots of tiny reads during stepping.

If PicoRVD connects while the chip is in reset, it can get hung.