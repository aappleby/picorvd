#include "SLDebugger.h"

//------------------------------------------------------------------------------

void test_unaligned_transfers(SLDebugger& sl) {
  auto& wire = sl.wire;

  uint8_t blank[16] = {0};
  uint8_t stripe[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  uint8_t buf[16] = {0};

  wire.set_block_unaligned(0x20000700, blank, 16);
  wire.set_block_unaligned(0x20000700 + 16, blank, 16);
  wire.set_block_unaligned(0x20000700 + 8, stripe, 8);

  for(int i = 0; i < 16; i++) {
    wire.get_block_unaligned(0x20000700 + i, buf, 16);
    for (int x = 0; x < 16; x++) printf("%02x", buf[x]);
    printf("\n");
  }

#if 0
  for(int i = 0; i < 8; i++) {
    wire.set_block_unaligned(0x20000700, blank, 16);
    wire.set_block_unaligned(0x20000700 + i, stripe, 8);
    wire.get_block_unaligned(0x20000700, buf, 16);
    for (int x = 0; x < 16; x++) printf("%02x", buf[x]);
    printf("\n");
  }
#endif

#if 0
  uint32_t buf[512];
  memset(buf, 0, sizeof(buf));
  wire.set_block_unaligned(0x20000000, buf, 2048);
  wire.get_block_unaligned(0x20000000, buf, 2048);

  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 8; x++) {
      printf("0x%08x ", buf[x + 8*y]);
    }
    printf("\n");
  }
#endif
}

//------------------------------------------------------------------------------
