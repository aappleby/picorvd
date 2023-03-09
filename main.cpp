#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "build/singlewire.pio.h"
#include "hardware/clocks.h"
#include "WCH_Debug.h"

const uint SWD_PIN = 14;

//-----------------------------------------------------------------------------

int main() {
  set_sys_clock_khz(120000, true);

  stdio_usb_init();
  while(!stdio_usb_connected());
  sleep_ms(100);

  SLDebugger sl;
  sl.init(SWD_PIN);

  printf("\033[2J");

  while(1) {
    sl.reset();
    sl.stop_watchdogs();

    //----------
#if 0
    uint32_t       data0;
    uint32_t       data1;
    Reg_DMCTRL     reg_dmctrl;
    Reg_DMSTATUS   reg_dmstatus;
    Reg_HARTINFO   reg_hartinfo;
    Reg_ABSTRACTCS reg_abstractcs;
    Reg_COMMAND    reg_command;
    Reg_AUTOCMD    reg_autocmd;
    Reg_HALTSUM0   reg_haltsum0;
    Reg_CPBR       reg_cpbr;
    Reg_CFGR       reg_cfgr;
    Reg_SHDWCFGR   reg_shdwcfgr;

    sl.halt();
    sl.get(ADDR_DATA0,      &data0);
    sl.get(ADDR_DATA1,      &data1);
    sl.get(ADDR_DMCONTROL,  &reg_dmctrl);
    sl.get(ADDR_DMCONTROL,  &reg_dmctrl);
    sl.get(ADDR_DMSTATUS,   &reg_dmstatus);
    sl.get(ADDR_HARTINFO,   &reg_hartinfo);
    sl.get(ADDR_ABSTRACTCS, &reg_abstractcs);
    sl.get(ADDR_COMMAND,    &reg_command);
    sl.get(ADDR_AUTOCMD,    &reg_autocmd);
    sl.get(ADDR_HALTSUM0,   &reg_haltsum0);
    sl.get(ADDR_CPBR,       &reg_cpbr);
    sl.get(ADDR_CFGR,       &reg_cfgr);
    sl.get(ADDR_SHDWCFGR,   &reg_shdwcfgr);

    uint32_t part0, part1, part2, part3, part4;
    sl.read_bus32(0x1FFFF7E0, &part0);
    sl.read_bus32(0x1FFFF7E8, &part1);
    sl.read_bus32(0x1FFFF7EC, &part2);
    sl.read_bus32(0x1FFFF7F0, &part3);
    sl.read_bus32(0x1FFFF7C4, &part4);
    sl.unhalt();

    static int rep = 0;

    printf("\033c");
    printf("rep    %d\n", rep++);
    printf("\n");

    printf("part0 0x%08x\n", part0);
    printf("part1 0x%08x\n", part1);
    printf("part2 0x%08x\n", part2);
    printf("part3 0x%08x\n", part3);
    printf("part4 0x%08x\n", part4);
    printf("\n");

    printf("data0 0x%08x\n", data0);
    printf("data1 0x%08x\n", data1);
    printf("\n");

    printf("reg_dmctrl\n");     reg_dmctrl.dump();     printf("\n");
    printf("reg_dmstatus\n");   reg_dmstatus.dump();   printf("\n");
    printf("reg_hartinfo\n");   reg_hartinfo.dump();   printf("\n");
    printf("reg_abstractcs\n"); reg_abstractcs.dump(); printf("\n");
    printf("reg_command\n");    reg_command.dump();    printf("\n");
    printf("reg_autocmd\n");    reg_autocmd.dump();    printf("\n");
    printf("reg_haltsum0\n");   reg_haltsum0.dump();   printf("\n");
    printf("reg_cpbr\n");       reg_cpbr.dump();       printf("\n");
    printf("reg_cfgr\n");       reg_cfgr.dump();       printf("\n");
    printf("reg_shdwcfgr\n");   reg_shdwcfgr.dump();   printf("\n");
#endif

    sl.halt();

    printf("\033c");
    for (int i = 0; i < 16; i++) {
      uint32_t r = sl.get_gpr(i);
      printf("gpr %02d = 0x%08x\n", i, r);
    }

    sl.unhalt();


    sleep_ms(30);
  }
}
