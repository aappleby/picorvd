#include <stdio.h>

#include "pico/stdlib.h"
#include "tusb.h"

#include "PicoSWIO.h"
#include "RVDebug.h"
#include "WCHFlash.h"
#include "SoftBreak.h"
#include "Console.h"
#include "GDBServer.h"  
#include "debug_defines.h"

const int PIN_SWIO = 28;
const int PIN_UART_TX = 0;
const int PIN_UART_RX = 1;
const int ch32v003_flash_size = 16*1024;

//------------------------------------------------------------------------------

int main() {
  // Enable non-USB serial port on gpio 0/1 for meta-debug output :D
  stdio_uart_init_full(uart0, 1000000, PIN_UART_TX, PIN_UART_RX);

  printf("\n\n\n");
  printf("//==============================================================================\n");
  printf("// PicoDebug 0.0.2\n");

  printf("//----------------------------------------\n");
  printf("// PicoSWIO\n");
  PicoSWIO* swio = new PicoSWIO();
  swio->reset(PIN_SWIO);
  printf("\n");
  swio->dump();
  printf("\n");

#if 0
  swio->put(DM_DMCONTROL, 0x80000001);
  while (!(swio->get(DM_DMSTATUS) & BIT_ALLHALTED)) {
    printf("not halted yet\n");
  }
  swio->put(DM_DMCONTROL, 0x00000001);

  swio->put(DM_DMCONTROL, 0x40000001);
  //while (!(swio->get(DM_DMSTATUS) & BIT_ALLRESUMEACK)) {
  while (!(swio->get(DM_DMSTATUS) & BIT_ALLRUNNING)) {
    printf("not resumed yet\n");
  }
  swio->put(DM_DMCONTROL, 0x00000001);

  Reg_DMSTATUS(swio->get(DM_DMSTATUS)).dump();
#endif


  printf("//----------------------------------------\n");
  printf("// RVDebug\n");
  RVDebug* rvd = new RVDebug(swio);

  swio->put(DM_DMCONTROL, BIT_DMACTIVE | BIT_ACKHAVERESET);

  //rvd->halt();
  //rvd->resume();

  rvd->reset_cpu();

  rvd->resume();

  rvd->dump();
  printf("\n");

  printf("ok\n");

#if 0

  printf("//----------------------------------------\n");
  printf("// WCHFlash\n");
  WCHFlash* flash = new WCHFlash(rvd, ch32v003_flash_size);
  flash->reset();
  printf("\n");
  rvd->halt();
  flash->dump();
  rvd->resume();
  printf("\n");

  printf("//----------------------------------------\n");
  printf("// SoftBreak\n");
  SoftBreak* soft = new SoftBreak(rvd, flash);
  soft->reset();
  soft->dump();

  printf("//----------------------------------------\n");
  printf("// Console\n");
  Console* console = new Console(rvd, flash, soft);
  console->reset();

  printf("//----------------------------------------\n");
  printf("// GDBServer\n");
  GDBServer* gdb = new GDBServer(rvd, flash, soft);
  gdb->reset();

  printf("//----------------------------------------\n");
  printf("// USB\n");
  tud_init(BOARD_TUD_RHPORT);

  printf("//----------------------------------------\n");
  printf("// Everything up and running!\n");

  while (1) {
    //----------------------------------------
    // Update TinyUSB

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

#endif

  return 0;
}

//------------------------------------------------------------------------------
