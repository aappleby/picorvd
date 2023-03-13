#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "build/singlewire.pio.h"
#include "hardware/clocks.h"
#include "WCH_Debug.h"

const uint SWD_PIN = 14;

//------------------------------------------------------------------------------

void mix(uint32_t& x) {
  x *= 0x1234567;
  x ^= x >> 16;
  x *= 0x1234567;
  x ^= x >> 16;
}

//------------------------------------------------------------------------------

int main() {
  set_sys_clock_khz(100000, true);

  stdio_usb_init();
  while(!stdio_usb_connected());
  sleep_ms(100);

  SLDebugger sl;
  sl.init(SWD_PIN);

  uint32_t temp[32];
  int rep = 0;

  printf("\033[2J");

  while(1) {
    sl.reset();
    sl.halt();
    sl.stop_watchdogs();

#if 1
    sl.save_regs();

    for (int i = 0; i < 16; i++) {
      sl.put_gpr(i, 0xCAFED00D);
    }

    Reg_ABSTRACTCS reg_abstractcs;
    sl.get(ADDR_ABSTRACTCS, &reg_abstractcs);

    sl.load_regs();

    static uint32_t temp[32];
    for (int i = 0; i < 16; i++) {
      temp[i] = sl.get_gpr(i);
    }

    sl.unhalt();

    printf("\033c");
    printf("rep %d\n", rep++);
    for (int i = 0; i < 16; i++) {
      printf("gpr %02d = 0x%08x\n", i, temp[i]);
    }
    printf("\n");

    printf("reg_abstractcs\n"); reg_abstractcs.dump(); printf("\n");

#endif

    sleep_ms(500);
  }
}
