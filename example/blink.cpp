#define SYSTEM_CORE_CLOCK 48000000
#include "ch32v003fun.h"

__attribute__((naked, noinline)) void prog_get_set_u32() {
  asm(R"(
    lui a0,0xe0000
    addi a0,a0,0xF4
    lw a1,4(a0)
    andi a1,a1,1
    beqz a1,get_u32
  set_u32:
    lw a1,4(a0)
    add a1, a1, -1
    lw a0,0(a0)
    sw a0,0(a1)
    ebreak
  get_u32:
    lw a1,4(a0)
    lw a1,0(a1)
    sw a1,0(a0)
    ebreak
  )");
};

__attribute__ ((noinline)) void busywait(int reps) {
  volatile int x = 0x1234;
  for (volatile int i = 0; i < reps; i++) {
    x *= 0x1234567;
    x ^= x >> 16;
    x += reps;
    x *= 0x1234567;
    x ^= x >> 16;
    x += reps;
  }
}

int main()
{
  SystemInit48HSI();

  prog_get_set_u32();

  // Enable GPIOD.
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOD;

  // GPIO D0 Push-Pull, 10MHz Slew Rate Setting
  GPIOD->CFGLR &= ~(0xf<<(4*0));
  GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP)<<(4*0);

  GPIOD->CFGLR &= ~(0xf<<(4*4));
  GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP)<<(4*4);

  for (int i = 0; i < 20; i++) {
    GPIOD->BSHR  = (1<<0) | (1<<4);   // Turn on GPIOD0
    busywait(2000);
    GPIOD->BSHR  = (1<<(16+0)) | (1<<(16+4)); // Turn off GPIOD0
    busywait(2000);
  }

  while(1)
  {
    GPIOD->BSHR = (1<<0) | (1<<4);   // Turn on GPIOD0
    busywait(10000);
    GPIOD->BSHR = (1<<(16+0)) | (1<<(16+4)); // Turn off GPIOD0
    busywait(10000);
  }
}
