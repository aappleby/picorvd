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


int main() {
  set_sys_clock_khz(100000, true);

  stdio_init_all();

  printf("\n");
  printf("----------------------------------------\n");
  printf("pch32 main(derp2)\n");

  int sm = 0;
  uint offset = pio_add_program(pio0, &singlewire_program);
  auto freq = 800000;

  pio_gpio_init(pio0, LED_PIN);
  pio_sm_set_consecutive_pindirs(pio0, sm, LED_PIN, 1, true);

  //pio_sm_config c = singlewire_program_get_default_config(offset);

  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, offset + singlewire_wrap_target, offset + singlewire_wrap);
  sm_config_set_sideset(&c, 1, false, false);

  sm_config_set_sideset_pins(&c, LED_PIN);
  sm_config_set_out_shift(&c, false, true, 32);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  int cycles_per_bit = 2 + 5 + 3;
  float div = clock_get_hz(clk_sys) / (freq * cycles_per_bit);
  sm_config_set_clkdiv(&c, div);

  pio_sm_init(pio0, sm, offset, &c);
  pio_sm_set_enabled(pio0, sm, true);



  while(1) {
    pio_sm_put_blocking(pio0, 0, 0xAADDDDAA);
    //while(pio_sm_get_tx_fifo_level(pio, sm));
    sleep_us(50);
  }


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