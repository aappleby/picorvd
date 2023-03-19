#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "build/singlewire.pio.h"
#include "hardware/clocks.h"
#include "WCH_Debug.h"

const uint SWD_PIN = 14;

extern "C" {
  void stdio_usb_out_chars(const char *buf, int length);
  int stdio_usb_in_chars(char *buf, int length);

  void uart_write_blocking (uart_inst_t *uart, const uint8_t *src, size_t len);
  void uart_read_blocking (uart_inst_t *uart, uint8_t *dst, size_t len);
};

//------------------------------------------------------------------------------

void usb_printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char buffer[256];
  int len = vsnprintf(buffer, 256, fmt, args);
  stdio_usb_out_chars(buffer, len);
}

void ser_printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char buffer[256];
  int len = vsnprintf(buffer, 256, fmt, args);
  uart_write_blocking(uart0, (uint8_t*)buffer, len);
}

//------------------------------------------------------------------------------

void mix(uint32_t& x) {
  x *= 0x1234567;
  x ^= x >> 16;
  x *= 0x1234567;
  x ^= x >> 16;
}

void test_thingy(SLDebugger& sl) {
  printf("testing thingy\n");

  sl.halt();
  bool lock1 = sl.is_flash_locked();
  sl.unlock_flash();
  bool lock2 = sl.is_flash_locked();
  sl.lock_flash();
  bool lock3 = sl.is_flash_locked();
  sl.unhalt();

  printf("l1 %d l2 %d l3 %d\n", lock1, lock2, lock3);
}

//------------------------------------------------------------------------------

void test_other_thingy(SLDebugger& sl) {
  printf("testing other thingy\n");

  uint32_t temp[32] = {0};

  sl.halt();

  sl.put_mem32(0x20000800 - 16, 0x5AA55AA5);
  sl.put_mem32(0x20000800 - 12, 0x5AA55AA5);
  sl.put_mem32(0x20000800 -  8, 0x5AA55AA5);
  sl.put_mem32(0x20000800 -  4, 0x5AA55AA5);

  temp[0] = 0xDEAD0001;
  temp[1] = 0xDEAD0002;
  temp[2] = 0xDEAD0003;
  temp[3] = 0xDEAD0004;

  sl.put_block32(0x20000800 - 16, temp, 4);

  for (int i = 0; i < 4; i++) temp[i] = 0;

  sl.get_block32(0x20000800 - 16, temp, 4);

  sl.unhalt();

  for (int i = 0; i < 4; i++) {
    printf("temp[%d] = 0x%08x\n", i, temp[i]);
  }
}

//------------------------------------------------------------------------------

void dump_ram(SLDebugger& sl) {
  uint32_t temp[512];

  sl.halt();
  sl.stop_watchdogs();
  sl.get_block32(0x20000000, temp, 512);
  sl.unhalt();

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 16; x++) {
      printf("0x%08x ", temp[x + y * 16]);
    }
    printf("\n");
  }
}

void dump_flash(SLDebugger& sl) {
  uint32_t temp[512];

  Reg_FLASH_STATR s;
  Reg_FLASH_CTLR c;
  Reg_FLASH_OBR o;
  uint32_t flash_wpr = 0;

  sl.halt();
  sl.get_mem32(ADDR_FLASH_WPR, &flash_wpr);
  sl.get_mem32(ADDR_FLASH_STATR, &s);
  sl.get_mem32(ADDR_FLASH_CTLR, &c);
  sl.get_mem32(ADDR_FLASH_OBR, &o);
  sl.get_block32(0x08000000, temp, 512);
  sl.unhalt();

  printf("flash_wpr 0x%08x\n", flash_wpr);
  printf("Reg_FLASH_STATR\n");
  s.dump();
  printf("Reg_FLASH_CTLR\n");
  c.dump();
  printf("Reg_FLASH_OBR\n");
  o.dump();

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 16; x++) {
      printf("0x%08x ", temp[x + y * 16]);
    }
    printf("\n");
  }
}

//------------------------------------------------------------------------------

void test_flash_write(SLDebugger& sl) {
  sl.halt();
  sl.stop_watchdogs();
  sl.unlock_flash();

  //printf("Starting flash write page\n");
  //sl.write_flash_word(0x08000000 + 1024, 0x5A5A);
  //sl.erase_flash_page(0x08000000 + 1024);
  /*
  for (int i = 0; i < 512; i++) {
    sl.write_flash_word(0x08000000 + (i * 2), 0x5A5A);
  }
  */
  //sl.erase_flash_page(0x08000000);
  /*
  for (int i = 0; i < 10; i++) {
    sl.write_flash_word(0x08000000 + (i * 2), 0x5A5A);
  }
  */
  //sl.write_flash_word(0x08000002, 0x5A5A);
  //sl.erase_flash_page(0x08000000);

  //sl.get_mem32(ADDR_FLASH_CTLR, &c);
  //c.PG = 1;
  //sl.put_mem32(ADDR_FLASH_CTLR, &c);
  //sl.put_mem16(0x08000000 + 64 + (0 * 2), 0x5A5A);

  if (1) {
    Reg_FLASH_CTLR c;
    sl.get_mem32(ADDR_FLASH_CTLR, &c);
    c.PG = 1;
    sl.put_mem32(ADDR_FLASH_CTLR, &c);

    /*
    // write flash
    sl.put_mem16(0x08000080, 0xCDCD);
    while (1) {
      Reg_FLASH_STATR s;
      sl.get_mem32(ADDR_FLASH_STATR, &s);
      if (!s.BUSY || s.EOP) break;
    }

    sl.put_mem16(0x08000082, 0xCDCD);
    while (1) {
      Reg_FLASH_STATR s;
      sl.get_mem32(ADDR_FLASH_STATR, &s);
      if (!s.BUSY || s.EOP) break;
    }
    */

    for (int i = 0; i < 32; i++) {
      sl.put_mem16(0x08000240 + (2 * i), 0xAACD);
      while (1) {
        Reg_FLASH_STATR s;
        sl.get_mem32(ADDR_FLASH_STATR, (void*)&s);
        printf("0x%08x\n", s.raw);
        if (!s.BUSY || s.EOP) break;
        //if (!s.BUSY) break;
      }
    }

    c.PG = 0;
    sl.put_mem32(ADDR_FLASH_CTLR, &c);
  }


  //sl.write_flash_word(0x08000000, 0x1313);
  //sl.write_flash_word(0x08000002, 0x5A5A);
  //sl.write_flash_word(0x08000000 + 64 + (2 * 2), 0x5A5A);
  //sl.write_flash_word(0x08000000 + 64 + (3 * 2), 0x5A5A);
  //printf("Flash write page done\n");

  sl.lock_flash();

  dump_flash(sl);

  sl.unhalt();
}

//------------------------------------------------------------------------------

void dump_csrs(SLDebugger& sl) {
  sl.halt();

  Reg_DCSR reg_dcsr;
  uint32_t dpc  = 0xDEADBEEF;
  uint32_t ds0  = 0xDEADBEEF;
  uint32_t ds1  = 0xDEADBEEF;

  sl.get_csr(0x7B0, &reg_dcsr);
  sl.get_csr(0x7B1, &dpc);
  sl.get_csr(0x7B2, &ds0);
  sl.get_csr(0x7B3, &ds1);

  Reg_DMCONTROL reg_dmctrl;
  Reg_DMSTATUS reg_dmstatus;
  Reg_ABSTRACTCS reg_abstractcs;
  sl.get(ADDR_DMCONTROL,  &reg_dmctrl);
  sl.get(ADDR_DMSTATUS,   &reg_dmstatus);
  sl.get(ADDR_ABSTRACTCS, &reg_abstractcs);

  sl.unhalt();

  printf("dpc  0x%08x\n", dpc);
  printf("ds0  0x%08x\n", ds0);
  printf("ds1  0x%08x\n", ds1);
  printf("\n");
  printf("reg_dcsr\n");       reg_dcsr.dump();       printf("\n");
  printf("reg_dmcontrol\n");  reg_dmctrl.dump();     printf("\n");
  printf("reg_dmstatus\n");   reg_dmstatus.dump();   printf("\n");
  printf("reg_abstractcs\n"); reg_abstractcs.dump(); printf("\n");
}

//------------------------------------------------------------------------------

int main() {
  stdio_usb_init();

  uart_init(uart0, 3000000);
  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);  

#if 0
  set_sys_clock_khz(100000, true);
  sleep_ms(100);
#endif

  while(1) {
    static int rep = 0;
    printf("Hello World usb %d\r\n", rep++);
    
    char buf[256];
    sprintf(buf, "Hello World uart %d\r\n", rep);
    uart_puts(uart0, buf);

    sleep_ms(100);
  }

#if 0
  SLDebugger sl;
  sl.init(SWD_PIN);
  sl.reset();

  char command[256] = {0};
  uint8_t cursor = 0;

  printf("\033[2J");
  while(1) {
    printf("> ");
    while(1) {
      auto c = getchar();
      //printf("%d\n", c);
      if (c == 8) {
        printf("\b \b");
        cursor--;
        command[cursor] = 0;
      }
      else if (c == '\n' || c == '\r') {
        command[cursor] = 0;
        printf("\n");
        break;
      }
      else {
        printf("%c", c);
        command[cursor++] = c;
      }
    }

    if (strcmp(command, "test1") == 0)      test_thingy(sl);
    if (strcmp(command, "test2") == 0)      test_other_thingy(sl);
    if (strcmp(command, "dump_csrs") == 0)  dump_csrs(sl);
    if (strcmp(command, "dump_ram") == 0)   dump_ram(sl);
    if (strcmp(command, "dump_flash") == 0) dump_flash(sl);
    if (strcmp(command, "test_flash_write") == 0) test_flash_write(sl);

    cursor = 0;
    command[cursor] = 0;
  }
#endif
}
