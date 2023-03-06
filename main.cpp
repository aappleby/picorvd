#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "build/singlewire.pio.h"
#include "hardware/clocks.h"

const uint LED_PIN = 15;
const uint LED_BIT = (1 << LED_PIN);

// 10 cycles per call + 5 cycles per x
__attribute__((noinline))
void delay(volatile int x) {
  while(x--);
}

#define DELAY_NS(x) delay((x - 100) / 50)

static volatile sio_hw_t* sio = (volatile sio_hw_t *)SIO_BASE;

void bitbang_send(uint8_t addr, uint32_t data);

void sl_put(uint8_t addr, uint32_t data) {
  addr = ((~addr) << 1) | 0;
  data = ~data;

  pio_sm_put_blocking(pio0, 0, addr);
  pio_sm_put_blocking(pio0, 0, data);
  pio_sm_get_blocking(pio0, 0);
}

uint32_t sl_get(uint8_t addr) {  
  addr = ((~addr) << 1) | 1;

  pio_sm_put_blocking(pio0, 0, addr);
  return pio_sm_get_blocking(pio0, 0);
}

int main() {
  set_sys_clock_khz(96000, true);

  //sleep_ms(4000);

  stdio_init_all();

  for (int i = 0; i < 1; i++) {
    printf("\n");
    printf("----------------------------------------\n");
    printf("pch32 main() %d\n", i);
  }
  

  const auto pad_hz = (0 << PADS_BANK0_GPIO0_DRIVE_LSB) | (0 << PADS_BANK0_GPIO0_SLEWFAST_LSB) | (1 << PADS_BANK0_GPIO1_IE_LSB);// | (1 << PADS_BANK0_GPIO1_SCHMITT_LSB);
  const auto pad_pu = pad_hz | (1 << PADS_BANK0_GPIO0_PUE_LSB);
  const auto pad_pd = pad_hz | (1 << PADS_BANK0_GPIO0_PDE_LSB);

  auto& pad = padsbank0_hw->io[LED_PIN];
  pad = pad_hz;


  // pio_sm_set_out_pins    (PIO pio, uint sm, uint out_base, uint out_count);
  // pio_sm_set_set_pins    (PIO pio, uint sm, uint set_base, uint set_count);
  // pio_sm_set_in_pins     (PIO pio, uint sm, uint in_base);
  // pio_sm_set_sideset_pins(PIO pio, uint sm, uint sideset_base);

  int sm = 0;
  uint offset = pio_add_program(pio0, &singlewire_program);
  printf("program at offset %d\n", offset);

  pio_gpio_init(pio0, LED_PIN);
  pio_sm_set_consecutive_pindirs(pio0, sm, LED_PIN, 1, /*is_out*/ true);

  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, offset + singlewire_wrap_target, offset + singlewire_wrap);
  sm_config_set_sideset(&c, 1, /*optional*/ false, /*pindirs*/ false);
  
  sm_config_set_out_pins    (&c, LED_PIN, 1);
  sm_config_set_in_pins     (&c, LED_PIN);
  sm_config_set_set_pins    (&c, LED_PIN, 1);
  sm_config_set_sideset_pins(&c, LED_PIN);

  sm_config_set_out_shift   (&c, /*shift_right*/ false, /*autopull*/ false, 32);
  sm_config_set_in_shift    (&c, /*shift_right*/ false, /*autopush*/ true, /*push_threshold*/ 32);

  sm_config_set_clkdiv(&c, 12);

  pio_sm_init(pio0, sm, offset, &c);


  //pio0->input_sync_bypass = 0xFFFFFFFF;





  int rep = 0;

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

  // wch-link sequence COLD BOOT
  // 2000 us low
  //    6 ns high
  // 0 0000001 0 10100101 01011010 11111011 11111111 // 
  // 0 0000010 0 10100101 01011010 11111011 11111111 // 
  // 0 0000011 1 11111111 11111110 11111011 11111100 // 
  // 0 1101110 1 11111111 10110011 11110011 01111101 // R 0x11 0x004C0C82
  // 0 1101111 0 01111111 11111111 11111111 11111110 // 
  // 0 1101111 0 01111111 11111111 11111111 11111110 // 
  // 0 1101001 1 11110111 11111111 11111111 11111101 // 
  
  // (5 msec idle)

  // 0 1101110 1 11111111 10110011 11111100 01111101 // R 0x11 0x004C0382
  // 0 0000000 1 11111111 11001111 11111010 11111111 // R 0x7F 0x00300500
  // 0 1111010 0 11100000 00000000 00001000 00111011 // W 0x05 0x1FFFF7C4

  // load the 32-bit memory read program
  // 0 1011111 0 10000100 11011010 11101111 10001100 // W 0x20 0x7B251073 // prog[0]
  // 0 1011110 0 10000100 11001010 01101111 10001100 // W 0x21 0x7B359073 // prog[1]
  // 0 1011101 0 00011111 11111111 11111010 11001000 // W 0x22 0xE0000537 // prog[2]
  // 0 1011100 0 11110000 01111010 11011010 01111100 // W 0x23 0x0F852583 // prog[3]
  // 0 1011011 0 11010101 11011100 10111110 01110011 // W 0x24 0x2A23418C // prog[4]
  // 0 1011010 0 11011010 10001100 11110001 01001010 // W 0x25 0x25730EB5 // prog[5]
  // 0 1011001 0 11011010 00001100 10000100 11011111 // W 0x26 0x25F37B20 // prog[6]
  // 0 1011000 0 01101111 11111101 10000100 11001111 // W 0x27 0x90027B30 // prog[7]

  // 0 1101000 0 11111111 11111011 11111111 11111111 // W 0x17 0x00040000
  // 0 1111011 1 11111111 11001111 11111010 11111111 // R 0x04 0x00300500
  // 0 1111011 0 11111111 11111111 11111111 11111100 // W 0x04 0x00000003
  // 0 1101000 0 11111111 11011100 11111000 00111111 // W 0x17 0x002307C0



  // leave reset - 
  // 0 1101111 0 10111111 11111111 11111111 11111110 // W 0x10 0x40000001
  // 0 1101110 1 11111111 10110000 11110011 01111101 // R 0x11 0x004F0C82


  // wch-link sequence WARM BOOT
  // op  1 - 0 0000001 0 10100101 01011010 11111011 11111111 : W 0x7E 0x5AA50400 // match
  // op  2 - 0 0000010 0 10100101 01011010 11111011 11111111 : W 0x7D 0x5AA50400 // match
  // op  3 - 0 0000011 1 11111111 11111110 11111011 11111100 : R 0x7C 0x00010403 // match
  // op  4 - 0 1101110 1 11111111 10110000 11111100 01111101 : R 0x11 0x004F0382 // 
  // op  5 - 0 1101111 0 01111111 11111111 11111111 11111110 : W 0x10 0x80000001
  // op  6 - 0 1101111 0 01111111 11111111 11111111 11111110 : W 0x10 0x80000001
  // op  7 - 0 1101001 1 11110111 11111111 11111111 11111101 : R 0x16 0x08000002
  // (5 msec idle)
  // op  8 - 0 1101110 1 11111111 10110000 11111100 01111101 : R 0x11 0x004F0382
  // op  9 - 0 0000000 1 11111111 11001111 11111010 11111111 : R 0x7F 0x00300500
  // op 10 - 0 1111010 0 11100000 00000000 00001000 00111011 : W 0x05 0x1FFFF7C4
  // op 11 - 0 1101000 0 11111111 11111011 11111111 11111111 : W 0x17 0x00040000
  // op 12 - 0 1111011 1 11111111 11001111 11111010 11111111 : R 0x04 0x00300500
  // op 13 - 0 1111011 0 11111111 11111111 11111111 11111100 : W 0x04 0x00000003
  // op 14 - 0 1101000 0 11111111 11011100 11111000 00111111 : W 0x17 0x002307C0

	// then another delay, then our command WWRWWRWWRWWRWWR

	// 1111010 0 11100000 00000000 00001000 00011111 0x05 DEG_Abst_DATA1 := 0x1FFFF7E0
	// 1101000 0 11111111 11111011 11111111 11111111 0x17 DEG_Abst_CMD   := 0x00040000
	// 1111011 1 00000000 00000000 11111111 11101111 0x04 DEG_Abst_DATA0 == 0xFFFF0010

	// 1111010 0 11100000 00000000 00001000 00010111 0x05 DEG_Abst_DATA1 := 0x1FFFF7E8
	// 1101000 0 11111111 11111011 11111111 11111111 0x17 DEG_Abst_CMD   := 0x0004 0000
	// 1111011 1 11000010 10000111 01010100 00110010 0x04 DEG_Abst_DATA0 == 0x3D78ABCD

	// 1111010 0 11100000 00000000 00001000 00010011 0x05 DEG_Abst_DATA1 := 0x1FFFF7EC
	// 1101000 0 11111111 11111011 11111111 11111111 0x17 DEG_Abst_CMD   := 0x0004 0000
	// 1111011 1 01011010 01110010 01000011 10110111 0x04 DEG_Abst_DATA0 == 0xA58DBC48

	// 11110100 W 11100000 00000000 00010000 00001111 F0 - part flags  = 0xffffffff
	///                                      00111011 C4 - part type b = 0x00300500

  pio_sm_set_enabled(pio0, sm, true);

  while(1) {
    printf("\n");
    printf("rep   %04d\n", rep++);

    // Grab pin and send a 2 msec low pulse to reset debug module (i think?)
    iobank0_hw->io[LED_PIN].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
    sio->gpio_clr    = LED_BIT;
    sio->gpio_oe_set = LED_BIT;
    sleep_ms(5);
    sio->gpio_oe_clr = LED_BIT;
    iobank0_hw->io[LED_PIN].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

                  sl_put(0x7E, 0x5AA50400);    sleep_us(5);
                  sl_put(0x7D, 0x5AA50400);    sleep_us(3);
    uint32_t r1 = sl_get(0x7C);                sleep_us(8);
    uint32_t r2 = sl_get(0x11);                sleep_us(10);
                  sl_put(0x10, 0x80000001);    sleep_us(6);
                  sl_put(0x10, 0x80000001);    sleep_us(4);
    uint32_t r3 = sl_get(0x16);
    
    sleep_us(5000);

    uint32_t r4 = sl_get(0x11);                sleep_us(9);
    uint32_t r5 = sl_get(0x7F);                sleep_us(11);
                  sl_put(0x05, 0x1FFFF7C4);    sleep_us(6);


    // load the 32-bit memory read program
    sl_put(0x20, 0x7B251073);
    sl_put(0x21, 0x7B359073);
    sl_put(0x22, 0xE0000537);
    sl_put(0x23, 0x0F852583);
    sl_put(0x24, 0x2A23418C);
    sl_put(0x25, 0x25730EB5);
    sl_put(0x26, 0x25F37B20);
    sl_put(0x27, 0x90027B30);

                  sl_put(0x17, 0x00040000);    sleep_us(4);
    uint32_t r6 = sl_get(0x04);                sleep_us(12);
                  sl_put(0x04, 0x00000003);    sleep_us(6);
                  sl_put(0x17, 0x002307C0);    sleep_us(500);

    // part reg read

    sl_put(0x05, 0x1FFFF7E0); sleep_us(10);
    sl_put(0x17, 0x00040000); sleep_us(10);
    uint32_t r7 = sl_get(0x04);

    sl_put(0x05, 0x1FFFF7E8); sleep_us(10);
    sl_put(0x17, 0x00040000); sleep_us(10);
    uint32_t r8 = sl_get(0x04);

    sl_put(0x05, 0x1FFFF7EC); sleep_us(10);
    sl_put(0x17, 0x00040000); sleep_us(10);
    uint32_t r9 = sl_get(0x04);

    sl_put(0x05, 0x1FFFF7F0); sleep_us(10);
    sl_put(0x17, 0x00040000); sleep_us(10);
    uint32_t r10 = sl_get(0x04);

    sl_put(0x05, 0x1FFFF7C4); sleep_us(10);
    sl_put(0x17, 0x00040000); sleep_us(10);
    uint32_t r11 = sl_get(0x04);

    printf("r1 0x00010403 = 0x%08x\n", r1);
    printf("r2 0x004F0382 = 0x%08x\n", r2);
    printf("r3 0x08000002 = 0x%08x\n", r3);
    printf("r4 0x004F0382 = 0x%08x\n", r4);
    printf("r5 0x00300500 = 0x%08x\n", r5);
    printf("r6 0x00300500 = 0x%08x\n", r6);
    printf("r7  0x%08x\n", r7);
    printf("r8  0x%08x\n", r8);
    printf("r9  0x%08x\n", r9);
    printf("r10 0x%08x\n", r10);
    printf("r11 0x%08x\n", r11);

    sleep_ms(1000);
  }

  // if wc-link unlocks chip first
  // ONLINE_CAPR_STA = 0x00010403
  // DEG_Module_STA  = 0x004f0c82
  // DEG_Abst_STA    = 0x08000302
  // DEG_Module_STA  = 0x004f0382
  // ???             = 0x00300500
  // DEG_Abst_DATA0  = 0x00300500
  // DEG_Abst_DATA0  = 0x3d78abcd
  // DEG_Module_STA  = 0x004f0c82

  // if pico talks to chip after por
  // ONLINE_CAPR_STA = 0x00010403 ok
  // DEG_Module_STA  = 0x004f0c82 ok
  // DEG_Abst_STA    = 0x08000302 ok 
  // DEG_Module_STA  = 0x004f0382 ok 
  // ???             = 0x00300500 ok
  // DEG_Abst_DATA0  = 0x0982021a xxx
  // DEG_Abst_DATA0  = 0x0982021a xxx
  // DEG_Module_STA  = 0x004f0c82 ok


  /*
  while(1) {
    printf("\n");
    printf("rep   %04d\n", rep++);

    iobank0_hw->io[LED_PIN].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
    sio->gpio_clr    = LED_BIT;
    sio->gpio_oe_set = LED_BIT;
    // low pulse before rw seems to be 2 msec
    sleep_ms(2);
    sio->gpio_oe_clr = LED_BIT;

    iobank0_hw->io[LED_PIN].ctrl = GPIO_FUNC_PIO0 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

    //pio_sm_set_pins_with_mask(pio0, sm, 0x00000000, 0xFFFFFFFF);
    //sleep_ms(1);
    //pio_sm_set_pins(pio0, sm, 1);
    //sleep_ms(2);
    //printf("derp\n");

    pio_sm_set_enabled(pio0, sm, true);
    slput(ONLINE_CFGR_SHAD, 0x5AA50400);
    slput(ONLINE_CFGR_MASK, 0x5AA50400);
    printf("ONLINE_CAPR_STA = 0x%08x\n", slget(ONLINE_CAPR_STA));
    printf("DEG_Module_STA  = 0x%08x\n", slget(DEG_Module_STA));

    // this is def "halt"
    slput(DEG_Module_CTRL, 0x80000001);
    slput(DEG_Module_CTRL, 0x80000001);
    printf("DEG_Abst_STA    = 0x%08x\n", slget(DEG_Abst_STA));

    printf("DEG_Module_STA  = 0x%08x\n", slget(DEG_Module_STA));
    printf("???             = 0x%08x\n", slget(0x7F));

    slput(DEG_Abst_DATA1, 0x1FFFF7C4);
    slput(DEG_Abst_CMD, 0x00040000);
    printf("DEG_Abst_DATA0  = 0x%08x\n", slget(DEG_Abst_DATA0));

    slput(DEG_Abst_DATA1, 0x1FFFF7E8);
    slput(DEG_Abst_CMD,   0x00040000);
    printf("DEG_Abst_DATA0  = 0x%08x\n", slget(DEG_Abst_DATA0));

		// 1101111 0 10111111 11111111 11111111 11111110 DEG_Module_CTRL := 0x40000001
		// 1101110 1 11111111 10110000 11110011 01111101 DEG_Module_STA  == 0x004F0C82

    // this is def "unhalt"
    slput(DEG_Module_CTRL, 0x40000001);
    printf("DEG_Module_STA  = 0x%08x\n", slget(DEG_Module_STA));

    pio_sm_set_enabled(pio0, sm, false);
    sleep_ms(1000);
  }
  */
}










#if 0
  // WS2811 test
  /*
  while (1) {
    const uint32_t colors[] = {
      0x010000,
      0x000100,
      0x000001,
      0x020000,
      0x000200,
      0x000002,
      0x040000,
      0x000400,
      0x000004,
      0x010101,
    };

    for (int j = 0; j < 10; j++) {
      uint32_t blah = colors[j];
      blah = 0xFFFFFFFF;
      for (int i = 0; i < 24; i++) {
        if (blah & (1 << (23 - i))) {
          gpio_put(LED_PIN, 1);
          DELAY_NS(700);
          gpio_put(LED_PIN, 0);
          DELAY_NS(600);
        }
        else {
          gpio_put(LED_PIN, 1);
          DELAY_NS(350);
          gpio_put(LED_PIN, 0);
          DELAY_NS(800);
        }
      }
    }
    gpio_put(LED_PIN, 0);
    DELAY_NS(60000);
  }
  */
#endif


#if 0
  const auto pad_hz = (3 << PADS_BANK0_GPIO0_DRIVE_LSB) | (1 << PADS_BANK0_GPIO0_SLEWFAST_LSB) | (1 << PADS_BANK0_GPIO1_IE_LSB) | (1 << PADS_BANK0_GPIO1_SCHMITT_LSB);
  const auto pad_pu = pad_hz | (1 << PADS_BANK0_GPIO0_PUE_LSB);
  const auto pad_pd = pad_hz | (1 << PADS_BANK0_GPIO0_PDE_LSB);

  auto& pad = padsbank0_hw->io[LED_PIN];
  pad = pad_pu;

  iobank0_hw->io[LED_PIN].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
     sio->gpio_oe_clr = LED_BIT;
    sio->gpio_set    = LED_BIT;

  volatile uint8_t  addr = 0b10110101;
  volatile uint32_t data = 0xA55A5A55;

  while(1) {
    //bitbang_send(addr, data);
  }
#endif


#if 0
__attribute__((noinline))
void bitbang_send(uint8_t addr, uint32_t data) {
  sio->gpio_oe_set = LED_BIT;
  sio->gpio_clr    = LED_BIT;
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop;");

  sio->gpio_set    = LED_BIT;
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop;");

  /*
  sio->gpio_clr    = LED_BIT;
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop;");

  sio->gpio_set    = LED_BIT;
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
  asm("nop; nop; nop; nop;");
  */
  for (int i = 7; i >= 0; i--) {
    if (addr & (1 << i)) {
      sio->gpio_clr    = LED_BIT;
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop;");

      sio->gpio_set    = LED_BIT;
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    }
    else {
      sio->gpio_clr    = LED_BIT;
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop;");

      sio->gpio_set    = LED_BIT;
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop;");
    }
  }

  for (int i = 31; i >= 0; i--) {
    if (data & (1 << i)) {
      sio->gpio_clr    = LED_BIT;
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop;");

      sio->gpio_set    = LED_BIT;
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    }
    else {
      sio->gpio_clr    = LED_BIT;
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop;");

      sio->gpio_set    = LED_BIT;
      asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
      asm("nop; nop; nop; nop; nop; nop; nop; nop;");
    }
  }


  sio->gpio_oe_clr = LED_BIT;
  DELAY_NS(3700);	
}
#endif