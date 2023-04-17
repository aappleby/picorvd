#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "build/singlewire.pio.h"
#include "hardware/clocks.h"
#include "SLDebugger.h"
#include "GDBServer.h"
#include "utils.h"
#include "WCH_Regs.h"
#include <assert.h>
#include <ctype.h>
#include "tusb.h"
#include "Console.h"

//------------------------------------------------------------------------------

static SLDebugger sl;
static GDBServer server;
static Console console;

const uint SWD_PIN = 28;

extern "C" {
  void uart_read_blocking (uart_inst_t *uart, uint8_t *dst, size_t len);
};

//------------------------------------------------------------------------------

int main() {
  // Enable non-USB serial port on gpio 0/1 for meta-debug output :D
  stdio_uart_init_full(uart0, 1000000, 0, 1);

  // Init USB
  tud_init(BOARD_TUD_RHPORT);

  printf("PicoDebug 0.0.2\n");

  sl.wire.init_pio(SWD_PIN);
  sl.reset_dbg();
  sl.wire.reset_dbg();

  printf("<starting gdb server>\n");
  server.init(&sl);
  server.state = GDBServer::IDLE;
  server.connected = false;

  printf("<starting console>\n");
  console.init(&sl);

  // Takes like 40 microseconds to poll DMSTATUS

  bool old_connected = false;

  while (1) {
    //----------------------------------------
    // Update TinyUSB

    tud_task();

    //----------------------------------------
    // Update debug interface status

    bool new_connected = tud_cdc_n_connected(0);

    if (!old_connected && new_connected) {
      //printf("GDB connected\n");
      sl.attach();
      sl.halt();
      server.on_connect();
    }
    
    if (old_connected && !new_connected) {
      //printf("GDB disconnected\n");
      server.on_disconnect();
      sl.detach();
    }

    if (sl.attached) {
      if (!sl.halted) {
        if (sl.wire.halted()) {
          //printf("\nCore halted due to breakpoint @ 0x%08x\n", sl.get_csr(CSR_DPC));
          sl.async_halted();
          if (server.connected) {
            server.send.set_packet("T05");
            server.state = GDBServer::SEND_PREFIX;
          }
        }
      }
    }

    //----------------------------------------
    // Update GDB stub

    if (server.connected) {
      bool usb_ie = tud_cdc_n_available(0); // this "available" check is required for some reason
      char usb_in = 0;
      if (usb_ie) {
        tud_cdc_n_read(0, &usb_in, 1);
      }

      char usb_out = 0;
      bool usb_oe = 0;
      server.update(usb_ie, usb_in, usb_out, usb_oe);

      if (usb_oe) {
        tud_cdc_n_write(0, &usb_out, 1);
        tud_cdc_n_write_flush(0);
      }
    }

    //----------------------------------------
    // Update uart console

    bool ser_ie = uart_is_readable(uart0);
    char ser_in = 0;
    if (ser_ie) uart_read_blocking(uart0, (uint8_t*)&ser_in, 1);
    console.update(ser_ie, ser_in);

    old_connected = new_connected;
  }


  return 0;
}

//------------------------------------------------------------------------------
