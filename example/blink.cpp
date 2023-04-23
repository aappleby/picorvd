// Could be defined here, or in the processor defines.
#define SYSTEM_CORE_CLOCK 48000000

#include "ch32v003fun.h"
#include <stdio.h>

#pragma GCC push_options
#pragma GCC optimize ("O0")
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
  /*
  static volatile int x = 0x1234;
  for (volatile int i = 0; i < reps; i++) {
    x *= 0x1234567;
    x ^= x >> 16;
    x ^= reps;
    x *= 0x1234567;
    x ^= x >> 16;
    x ^= reps;
    x *= 0x1234567;
    x ^= x >> 16;
    x ^= reps;
    x *= 0x1234567;
    x ^= x >> 16;
    x ^= reps;
  }
  return x;
  */
}
#pragma GCC pop_options

int main()
{
	SystemInit48HSI();

	// Enable GPIOD.
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOD;

	// GPIO D0 Push-Pull, 10MHz Slew Rate Setting
	GPIOD->CFGLR &= ~(0xf<<(4*0));
	GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP)<<(4*0);

	GPIOD->CFGLR &= ~(0xf<<(4*4));
	GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP)<<(4*4);

	for (int i = 0; i < 20; i++) {
		GPIOD->BSHR  = (1<<0) | (1<<4);	 // Turn on GPIOD0
    busywait(10000);
		GPIOD->BSHR  = (1<<(16+0)) | (1<<(16+4)); // Turn off GPIOD0
    busywait(10000);
	}

	while(1)
	{
		GPIOD->BSHR = (1<<0) | (1<<4);	 // Turn on GPIOD0
    busywait(40000);
		GPIOD->BSHR = (1<<(16+0)) | (1<<(16+4)); // Turn off GPIOD0
    busywait(40000);
	}
}
