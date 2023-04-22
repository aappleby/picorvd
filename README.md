This repo contains a GDB-compatible debugger for the RISC-V based CH32V003 series of chips by WinChipHead.

The app runs on a Raspberry Pi Pico and provides an implementation of the GDB remote stub that can communicate with the CH32V via WCH's semi-proprietary single-wire (SWIO) interface.

It allows you to program and debug a CH32V chip without needing the official WCH-Link dongle or a modified copy of OpenOCD.

To use, connect pin PD0 on your CH32V device to the Pico's SWIO pin (defaults to pin GP28) and add a 1Kohm pull-up resistor from SWIO to +3.3v.

After that run "gdb-multiarch {your_binary.elf}"

```
set debug remote 1
target extended-remote /dev/ttyACM1
```

NOTE - This repo is not ready for public consumption yet, I need to fix up some issues created during refactoring and merge the standalone blink example into this repo for testing.
