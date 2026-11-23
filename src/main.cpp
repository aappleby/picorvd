#include <stdio.h>

#include "pico/stdlib.h"
#include "tusb.h"

#include "PicoSWIO.h"
#include "RVDebug.h"
#include "WCHFlash.h"
#include "Breakpoints.h"
#include "Target.h"
#include "Console.h"
#include "GDBServer.h"
#include "debug_defines.h"
#include "utils.h"

const int PIN_SWIO = 28;
const int PIN_UART_TX = 0;
const int PIN_UART_RX = 1;

// Selected with the CMake option PICORVD_TARGET (see Target.cpp).
#ifndef PICORVD_TARGET
#define PICORVD_TARGET target_ch32v003
#endif
static const Target& target = PICORVD_TARGET;

void delay_us(int us) {
  auto now = time_us_32();
  while(time_us_32() < (now + us));
}

//------------------------------------------------------------------------------

int main() {
  // Enable non-USB serial port on gpio 0/1 for meta-debug output :D
  stdio_uart_init_full(uart0, 1000000, PIN_UART_TX, PIN_UART_RX);

  printf_g("\n\n\n");
  printf_g("//==============================================================================\n");
  printf_g("// PicoRVD 0.0.2 for %s\n\n", target.name);

  printf_g("// Starting PicoSWIO\n");
  PicoSWIO* swio = new PicoSWIO();
  swio->reset(PIN_SWIO, target.swio_clkdiv);

  printf_g("// Starting RVDebug\n");
  RVDebug* rvd = new RVDebug(swio, target.gpr_count);
  rvd->init();
  //rvd->dump();

  printf_g("// Starting WCHFlash\n");
  WCHFlash* flash = target.create_flash(rvd);
  flash->reset();
  //flash->dump();

  printf_g("// Starting breakpoints\n");
  Breakpoints* soft = target.create_breakpoints(rvd, flash);
  soft->init();
  //soft->dump();

  printf_g("// Starting GDBServer\n");
  GDBServer* gdb = new GDBServer(rvd, flash, soft, target.memory_map);
  gdb->reset();
  //gdb->dump();

  printf_g("// Starting Console\n");
  Console* console = new Console(rvd, flash, soft);
  console->reset();
  //console->dump();

  printf_g("// Everything up and running!\n");

  console->start();
  while (1) {

    //----------------------------------------
    // Update TinyUSB

    static bool tud_init_done = false;
    if (!tud_init_done) {
      tud_init(BOARD_TUD_RHPORT);
      tud_init_done = true;
    }
    tud_task();

    //----------------------------------------
    // Update GDB stub

    bool connected = tud_cdc_n_connected(0);
    bool usb_ie = tud_cdc_n_available(0); // this "available" check is required for some reason
    char usb_in = 0;
    char usb_out = 0;
    bool usb_oe = 0;

    if (usb_ie) {
      tud_cdc_n_read(0, &usb_in, 1);
    }
    gdb->update(connected, usb_ie, usb_in, usb_oe, usb_out);

    if (usb_oe) {
      tud_cdc_n_write(0, &usb_out, 1);
      tud_cdc_n_write_flush(0);
    }

    //----------------------------------------
    // Update uart console

    bool ser_ie = uart_is_readable(uart0);
    char ser_in = 0;

    if (ser_ie) {
      uart_read_blocking(uart0, (uint8_t*)&ser_in, 1);
    }
    console->update(ser_ie, ser_in);
  }

  return 0;
}

//------------------------------------------------------------------------------
