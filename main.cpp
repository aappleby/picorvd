#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "build/singlewire.pio.h"
#include "hardware/clocks.h"

const uint LED_PIN = 15;
static volatile sio_hw_t* sio = (volatile sio_hw_t *)SIO_BASE;

//-----------------------------------------------------------------------------

void sl_put(uint8_t addr, uint32_t data) {
  addr = ((~addr) << 1) | 0;
  data = ~data;
  pio_sm_put_blocking(pio0, 0, addr);
  pio_sm_put_blocking(pio0, 0, data);
}

uint32_t sl_get(uint8_t addr) {  
  addr = ((~addr) << 1) | 1;
  pio_sm_put_blocking(pio0, 0, addr);
  return pio_sm_get_blocking(pio0, 0);
}

//-----------------------------------------------------------------------------

int main() {
  set_sys_clock_khz(120000, true);

  stdio_init_all();

  int actual = 0;
  //actual = uart_set_baudrate(uart0, 256000);
  actual = uart_set_baudrate(uart0, 3000000);

  printf("\n");
  printf("----------------------------------------\n");
  printf("pch32 main()\n");

  const auto pad_hz = (0 << PADS_BANK0_GPIO0_DRIVE_LSB) | (0 << PADS_BANK0_GPIO0_SLEWFAST_LSB) | (1 << PADS_BANK0_GPIO1_IE_LSB);// | (1 << PADS_BANK0_GPIO1_SCHMITT_LSB);
  const auto pad_pu = pad_hz | (1 << PADS_BANK0_GPIO0_PUE_LSB);
  const auto pad_pd = pad_hz | (1 << PADS_BANK0_GPIO0_PDE_LSB);

  auto& pad = padsbank0_hw->io[LED_PIN];
  pad = pad_hz;

  int sm = 0;
  uint offset = pio_add_program(pio0, &singlewire_program);
  //printf("program at offset %d\n", offset);

  gpio_set_function(LED_PIN, GPIO_FUNC_PIO0);

  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap        (&c, offset + singlewire_wrap_target, offset + singlewire_wrap);
  sm_config_set_sideset     (&c, 1, /*optional*/ false, /*pindirs*/ false);
  sm_config_set_out_pins    (&c, LED_PIN, 1);
  sm_config_set_in_pins     (&c, LED_PIN);
  sm_config_set_set_pins    (&c, LED_PIN, 1);
  sm_config_set_sideset_pins(&c, LED_PIN);
  sm_config_set_out_shift   (&c, /*shift_right*/ false, /*autopull*/ false, 32);
  sm_config_set_in_shift    (&c, /*shift_right*/ false, /*autopush*/ true, /*push_threshold*/ 32);
  sm_config_set_clkdiv      (&c, 15);

  pio_sm_init(pio0, sm, offset, &c);
  pio_sm_set_enabled(pio0, sm, true);

  const uint8_t DEG_Abst_DATA0   = 0x04;
  const uint8_t DEG_Abst_DATA1   = 0x05;
  const uint8_t DEG_Module_CTRL  = 0x10;
  const uint8_t DEG_Module_STA   = 0x11;
  const uint8_t DEG_Hart_INFO    = 0x12;
  const uint8_t DEG_Abst_STA     = 0x16;
  const uint8_t DEG_Abst_CMD     = 0x17;
  const uint8_t DEG_Abst_CMDAUTO = 0x18;
  const uint8_t DEG_Prog_BUF0    = 0x20;
  const uint8_t DEG_Prog_BUF1    = 0x21;
  const uint8_t DEG_Prog_BUF2    = 0x22;
  const uint8_t DEG_Prog_BUF3    = 0x23;
  const uint8_t DEG_Prog_BUF4    = 0x24;
  const uint8_t DEG_Prog_BUF5    = 0x25;
  const uint8_t DEG_Prog_BUF6    = 0x26;
  const uint8_t DEG_Prog_BUF7    = 0x27;
  const uint8_t DEG_Hart_SUM0    = 0x40;
  const uint8_t ONLINE_CAPR_STA  = 0x7C;
  const uint8_t ONLINE_CFGR_MASK = 0x7D;
  const uint8_t ONLINE_CFGR_SHAD = 0x7E;

  printf("\033[2J");

  while(1) {
    // Grab pin and send a 2 msec low pulse to reset debug module (i think?)
    gpio_set_function(LED_PIN, GPIO_FUNC_SIO);
    sio->gpio_clr    = (1 << LED_PIN);
    sio->gpio_oe_set = (1 << LED_PIN);
    sleep_ms(2);
    sio->gpio_set    = (1 << LED_PIN);
    sleep_us(1);
    //sio->gpio_oe_clr = (1 << LED_PIN);
    gpio_set_function(LED_PIN, GPIO_FUNC_PIO0);

    // Turn on debugger output
    sl_put(ONLINE_CFGR_SHAD, 0x5AA50400);
    sl_put(ONLINE_CFGR_MASK, 0x5AA50400);
    uint32_t r1 = sl_get(ONLINE_CAPR_STA);
    uint32_t r2 = sl_get(DEG_Module_STA);

    // Halt core
    sl_put(DEG_Module_CTRL, 0x80000001);
    sl_put(DEG_Module_CTRL, 0x80000001);
    uint32_t r3 = sl_get(DEG_Abst_STA);
    uint32_t r4 = sl_get(DEG_Module_STA);
    uint32_t r5 = sl_get(0x7F);

    // load the 32-bit memory read program
    sl_put(0x20, 0x7B251073);
    sl_put(0x21, 0x7B359073);
    sl_put(0x22, 0xE0000537);
    sl_put(0x23, 0x0F852583);
    sl_put(0x24, 0x2A23418C);
    sl_put(0x25, 0x25730EB5);
    sl_put(0x26, 0x25F37B20);
    sl_put(0x27, 0x90027B30);

    // read part type
    sl_put(DEG_Abst_DATA1, 0x1FFFF7C4);
    sl_put(DEG_Abst_CMD,   0x00040000);
    uint32_t r6 = sl_get(DEG_Abst_DATA0);

    //----------
    // stop watchdogs

    sl_put(DEG_Abst_DATA0, 0x00000003);
    sl_put(DEG_Abst_CMD,   0x002307C0);

    //----------
    // part reg read

    sl_put(DEG_Abst_DATA1, 0x1FFFF7E0);
    sl_put(DEG_Abst_CMD,   0x00040000);
    uint32_t r7 = sl_get(DEG_Abst_DATA0);

    sl_put(DEG_Abst_DATA1, 0x1FFFF7E8);
    sl_put(DEG_Abst_CMD,   0x00040000);
    uint32_t r8 = sl_get(DEG_Abst_DATA0);

    sl_put(DEG_Abst_DATA1, 0x1FFFF7EC);
    sl_put(DEG_Abst_CMD,   0x00040000);
    uint32_t r9 = sl_get(DEG_Abst_DATA0);

    sl_put(DEG_Abst_DATA1, 0x1FFFF7F0);
    sl_put(DEG_Abst_CMD,   0x00040000);
    uint32_t r10 = sl_get(DEG_Abst_DATA0);

    sl_put(DEG_Abst_DATA1, 0x1FFFF7C4);
    sl_put(DEG_Abst_CMD,   0x00040000);
    uint32_t r11 = sl_get(DEG_Abst_DATA0);

    //----------
    // unhalt

    sl_put(DEG_Module_CTRL, 0x40000001);    
    uint32_t r12 = sl_get(DEG_Module_STA);

    //----------

    static int rep = 0;

    printf("\033c");
    printf("rep    %04d\n", rep++);
    printf("actual %d\n", actual);
    printf("r1  0x%08x ?= 0x00010403\n", r1);
    printf("r2  0x%08x ?= 0x004f0c82\n", r2);
    printf("r3  0x%08x ?= 0x08000002\n", r3);
    printf("r4  0x%08x ?= 0x004f0382\n", r4);
    printf("r5  0x%08x ?= 0x00300500\n", r5);
    printf("r6  0x%08x ?= 0x00300500\n", r6);
    printf("r7  0x%08x\n", r7);
    printf("r8  0x%08x\n", r8);
    printf("r9  0x%08x\n", r9);
    printf("r10 0x%08x\n", r10);
    printf("r11 0x%08x\n", r11);
    printf("r12 0x%08x\n", r12);

    //sleep_ms(100);
    //sleep_ms(1000);
  }
}
