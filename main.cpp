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

//------------------------------------------------------------------------------

void on_halt(SLDebugger& sl) {
  sl.halt();
}

void on_unhalt(SLDebugger& sl) {
  sl.unhalt();
}

//------------------------------------------------------------------------------

void on_status(SLDebugger& sl) {
  ser_printf("on_status()\n");

  Reg_CPBR       reg_cpbr;
  Reg_CFGR       reg_cfgr;
  Reg_SHDWCFGR   reg_shdwcfgr;
  Reg_DMCONTROL  reg_dmctrl;
  Reg_DMSTATUS   reg_dmstatus;
  Reg_ABSTRACTCS reg_abstractcs;

  Reg_DCSR dcsr;
  uint32_t dpc  = 0xDEADBEEF;
  uint32_t ds0  = 0xDEADBEEF;
  uint32_t ds1  = 0xDEADBEEF;

  //----------

  sl.halt();

  sl.getp(ADDR_CPBR,       &reg_cpbr);
  sl.getp(ADDR_CFGR,       &reg_cfgr);
  sl.getp(ADDR_SHDWCFGR,   &reg_shdwcfgr);
  sl.getp(ADDR_DMCONTROL,  &reg_dmctrl);
  sl.getp(ADDR_DMSTATUS,   &reg_dmstatus);
  sl.getp(ADDR_ABSTRACTCS, &reg_abstractcs);

  sl.get_csr(0x7B0, &dcsr);
  sl.get_csr(0x7B1, &dpc);
  sl.get_csr(0x7B2, &ds0);
  sl.get_csr(0x7B3, &ds1);

  sl.unhalt();

  //----------

  //usb_printf("reg_cpbr\n");       reg_cpbr.dump();       printf("\n");
  //usb_printf("reg_cfgr\n");       reg_cfgr.dump();       printf("\n");
  //usb_printf("reg_shdwcfgr\n");   reg_shdwcfgr.dump();   printf("\n");
  usb_printf("reg_dmcontrol\n");  reg_dmctrl.dump();     usb_printf("\n");
  usb_printf("reg_dmstatus\n");   reg_dmstatus.dump();   usb_printf("\n");
  usb_printf("reg_abstractcs\n"); reg_abstractcs.dump(); usb_printf("\n");
  usb_printf("reg_dcsr\n");       dcsr.dump();       usb_printf("\n");
  usb_printf("dpc  0x%08x\n", dpc);
  usb_printf("ds0  0x%08x\n", ds0);
  usb_printf("ds1  0x%08x\n", ds1);
  usb_printf("\n");

  ser_printf("on_status() done\n");
}

//------------------------------------------------------------------------------

uint16_t prog_run_task[16] = {
  0xc94c, // sw      a1,20(a0)
  0xc910, // sw      a2,16(a0)
  0xc914, // sw      a3,16(a0)
  0x455c, // lw      a5,12(a0)
  0x8b85, // andi    a5,a5,1
  0xfff5, // bnez    a5,17c <_Z14do_flash_thingRV9Reg_FLASHPvmm+0x6>
  0x2823, // sw      zero,16(a0)
  0x0005, //
  0x9002, // ebreak
  0x9002, // ebreak
  0x9002, // ebreak
  0x9002, // ebreak
  0x9002, // ebreak
  0x9002, // ebreak
  0x9002, // ebreak
  0x9002, // ebreak
};

/*
__attribute__((noinline))
void do_flash_thing(volatile Reg_FLASH& flash, void* addr, uint32_t c1, uint32_t c2) {
  flash.addr = (uint32_t)addr;
  flash.ctlr.raw = c1;
  flash.ctlr.raw = c2;
  while(flash.statr.BUSY);
  flash.ctlr.raw = 0;
}
*/

void on_wipe_chip(SLDebugger& sl) {
  ser_printf("on_wipe_chip()\n");

  sl.halt();
  sl.unlock_flash();

  // a0 = flash
  // a1 = addr
  // a2 = BIT_CTLR_MER
  // a3 = BIT_CTLR_MER | BIT_CTLR_STRT

  sl.load_prog((uint32_t*)prog_run_task);
  sl.put_gpr(10, 0x40022000);
  sl.put_gpr(11, 0);
  sl.put_gpr(12, BIT_CTLR_MER);
  sl.put_gpr(13, BIT_CTLR_MER | BIT_CTLR_STRT);
  sl.put(ADDR_COMMAND, 0x00040000);
  while(sl.get_abstatus().BUSY);

  sl.lock_flash();
  sl.unhalt();

  ser_printf("on_wipe_chip() done\n");
}

/*
> write_page
Command took 150523 us
> write_page
Command took 111231 us
> write_page
Command took 111209 us
> 

Command took 184551 us
> 
Command took 4 us
> write_page
Command took 117198 us
> write_page
Command took 117121 us
> 

*/

//------------------------------------------------------------------------------

uint16_t prog_write_incremental[16] = {
  // copy word and trigger BUFLOAD
  0x4180, // lw      s0,0(a1)
  0xc200, // sw      s0,0(a2)
  0xc914, // sw      a3,16(a0)

  // wait for ack
  0x4540, // lw      s0,12(a0)
  0x8805, // andi    s0,s0,1
  0xfc75, // bnez    s0,15a <waitloop1>

  // advance dest pointer and trigger START if we ended a page
  0x0611, // addi    a2,a2,4
  0x7413, // andi    s0,a2,63
  0x03f6, //
  0xe419, // bnez    s0,174 <end>
  0xc918, // sw      a4,16(a0)

  // wait for write to complete
  0x4540, // lw      s0,12(a0)
  0x8805, // andi    s0,s0,1
  0xfc75, // bnez    s0,16a <waitloop2>

  // reset buffer
  0xc91c, // sw      a5,16(a0)

  // update address
  0xc950, // sw      a2,20(a0)
};

void on_write_page(SLDebugger& sl, uint32_t dst_addr) {
  ser_printf("on_write_page()\n");

  sl.halt();
  sl.unlock_flash();

  sl.put_mem32(ADDR_FLASH_ADDR, dst_addr);
  sl.put_mem32(ADDR_FLASH_CTLR, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);

  sl.load_prog((uint32_t*)prog_write_incremental);
  sl.put_gpr(10, 0x40022000);
  sl.put_gpr(11, 0xE00000F4);
  sl.put_gpr(12, dst_addr);
  sl.put_gpr(13, BIT_CTLR_FTPG | BIT_CTLR_BUFLOAD);
  sl.put_gpr(14, BIT_CTLR_FTPG | BIT_CTLR_STRT);
  sl.put_gpr(15, BIT_CTLR_FTPG | BIT_CTLR_BUFRST);
 
  sl.put(ADDR_AUTOCMD, 1);
  bool first_word = true;

  int counter = 0;
  for (int page = 0; page < 3; page++) {
    for (int i = 0; i < 16; i++) {
      sl.put(ADDR_DATA0,   0xBEEF0000 + counter++);
      if (first_word) {
        sl.put(ADDR_COMMAND, 0x00040000);
        first_word = false;
      }
    }
    while(sl.get_abstatus().BUSY);
  }

  sl.put_mem32(ADDR_FLASH_CTLR, 0);

  sl.lock_flash();
  sl.unhalt();

  ser_printf("on_write_page() done\n");
}

//------------------------------------------------------------------------------

void test_thingy(SLDebugger& sl) {
  usb_printf("testing thingy\n");

  sl.halt();
  bool lock1 = sl.is_flash_locked();
  sl.unlock_flash();
  bool lock2 = sl.is_flash_locked();
  sl.lock_flash();
  bool lock3 = sl.is_flash_locked();
  sl.unhalt();

  usb_printf("l1 %d l2 %d l3 %d\n", lock1, lock2, lock3);
}

//------------------------------------------------------------------------------

void test_other_thingy(SLDebugger& sl) {
  usb_printf("testing other thingy\n");

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
    usb_printf("temp[%d] = 0x%08x\n", i, temp[i]);
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
      usb_printf("0x%08x ", temp[x + y * 16]);
    }
    usb_printf("\n");
  }
}

void on_dump_flash(SLDebugger& sl) {
  uint32_t temp[256];

  Reg_FLASH_STATR s;
  Reg_FLASH_CTLR c;
  Reg_FLASH_OBR o;
  uint32_t flash_wpr = 0;

  sl.halt();
  sl.get_mem32p(ADDR_FLASH_WPR, &flash_wpr);
  sl.get_mem32p(ADDR_FLASH_STATR, &s);
  sl.get_mem32p(ADDR_FLASH_CTLR, &c);
  sl.get_mem32p(ADDR_FLASH_OBR, &o);
  sl.get_block32(0x08000000, temp, 256);
  sl.unhalt();

  usb_printf("flash_wpr 0x%08x\n", flash_wpr);
  usb_printf("Reg_FLASH_STATR\n");
  s.dump();
  usb_printf("Reg_FLASH_CTLR\n");
  c.dump();
  usb_printf("Reg_FLASH_OBR\n");
  o.dump();

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      usb_printf("0x%08x ", temp[x + y * 16]);
    }
    usb_printf("\n");
  }
}

//------------------------------------------------------------------------------

int main() {
  set_sys_clock_khz(100000, true);
  stdio_usb_init();
  while(!stdio_usb_connected());

  uart_init(uart0, 3000000);
  gpio_set_function(0, GPIO_FUNC_UART);
  gpio_set_function(1, GPIO_FUNC_UART);  

  // Wait for Minicom to finish connecting before sending clear screen etc
  sleep_ms(100);

  SLDebugger sl;
  sl.init(SWD_PIN);
  sl.reset();
  sl.halt();

  char command[256] = {0};
  uint8_t cursor = 0;

  usb_printf("\033[2J");
  usb_printf("PicoDebug 0.0.0\n");
  ser_printf("<debug log begin>\n");

  while(1) {
    usb_printf("> ");
    while(1) {
      auto c = getchar();
      //usb_printf("%d\n", c);
      if (c == 8) {
        usb_printf("\b \b");
        cursor--;
        command[cursor] = 0;
      }
      else if (c == '\n' || c == '\r') {
        command[cursor] = 0;
        usb_printf("\n");
        break;
      }
      else {
        usb_printf("%c", c);
        command[cursor++] = c;
      }
    }

    uint32_t time_a = time_us_32();

    if (strcmp(command, "halt") == 0)       on_halt(sl);
    if (strcmp(command, "status") == 0)     on_status(sl);
    if (strcmp(command, "dump_flash") == 0) on_dump_flash(sl);
    if (strcmp(command, "wipe_chip") == 0)  on_wipe_chip(sl);
    if (strcmp(command, "write_page") == 0) on_write_page(sl, 0x08000000);

    uint32_t time_b = time_us_32();
    usb_printf("Command took %d us\n", time_b - time_a);

    //if (strcmp(command, "test1") == 0)      test_thingy(sl);
    //if (strcmp(command, "test2") == 0)      test_other_thingy(sl);
    //if (strcmp(command, "dump_csrs") == 0)  dump_csrs(sl);
    //if (strcmp(command, "dump_ram") == 0)   dump_ram(sl);
    //if (strcmp(command, "test_flash_write") == 0) test_flash_write(sl);

    cursor = 0;
    command[cursor] = 0;
  }
}
