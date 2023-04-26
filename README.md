# PicoRVD 

This repo contains a GDB-compatible remote debug interface for the RISC-V based CH32V003 series of chips by WinChipHead.

It allows you to program and debug a CH32V003 chip without needing the proprietary WCH-Link dongle or a modified copy of OpenOCD.

The app runs on a Raspberry Pi Pico and can communicate with the CH32V003 via WCH's semi-proprietary single-wire (SWIO) interface.

**This tool is very, very alpha - beware of bugs**

## Repo Structure

**src** is the main codebase. It compiles using the Pico SDK + CMake.

**src/singlewire.pio** is the Pico PIO code that generates SWIO waveforms on GP28

**example** contains a trivial blink example that builds using cnlohr's "ch32v003fun" library. You will need gcc-riscv64-unknown-elf installed to build. See build.sh/flash.sh/debug.sh for basic usage.

**test** contains just a simple tests to exercise aligned and unaligned reads/writes via the debug interface.

## Usage

Connect pin PD1 on your CH32V device to the Pico's SWIO pin (defaults to pin GP28), connect CH32V ground to Pico ground, and add a 1Kohm pull-up resistor from SWIO to +3.3v.

After that run "gdb-multiarch {your_binary.elf}" and type "target extended-remote /dev/ttyACM0" to connect to the debugger (replace ttyACM0 with whatever port your Pico shows up as).

Most operations should be faster than the WCH-Link by virtue of doing some basic Pico-side caching and avoiding redundant debug register read/writes.

Not all GDB remote functionality is implemented, but read/write of RAM, erasing/writing flash, setting breakpoints, and stepping should all work. The target chip can be reset via "monitor reset".

## Building:

Make sure PICO_SDK_PATH is set in your environment and run "build.sh" to compile.

Run "upload.sh" to upload the app to your Pico if it's connected to a Pico Debug Probe, or just use the standard hold-reset-and-reboot to mount your Pico as a flash drive and then copy bin/picorvd.uf2 to it.

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
Connect via "minicom -b 1000000 -D /dev/ttyACM0" (replace ttyACM0 with your debug probe port) and type "help" to get a list of commands.
