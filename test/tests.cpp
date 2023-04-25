#include "RVDebug.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>


void run_tests(RVDebug& rvd) {
  printf("Running tests...\n");

  rvd.reset();

  CHECK(rvd.get_abstractcs().CMDER == 0);

  uint32_t base = 0x20000400;

  // Test misaligned reads
  for (int offset = 0; offset < 4; offset++) {
    for (int i = 0; i < 8; i++) {
      rvd.set_mem_u8(base + i + offset, i + 1);
    }

    CHECK(rvd.get_mem_u32(base + 0 + offset) == 0x04030201);
    CHECK(rvd.get_mem_u32(base + 4 + offset) == 0x08070605);

    CHECK(rvd.get_mem_u16(base + 0 + offset) == 0x0201);
    CHECK(rvd.get_mem_u16(base + 2 + offset) == 0x0403);
    CHECK(rvd.get_mem_u16(base + 4 + offset) == 0x0605);
    CHECK(rvd.get_mem_u16(base + 6 + offset) == 0x0807);

    CHECK(rvd.get_mem_u8 (base + 0 + offset) == 0x01);
    CHECK(rvd.get_mem_u8 (base + 1 + offset) == 0x02);
    CHECK(rvd.get_mem_u8 (base + 2 + offset) == 0x03);
    CHECK(rvd.get_mem_u8 (base + 3 + offset) == 0x04);
    CHECK(rvd.get_mem_u8 (base + 4 + offset) == 0x05);
    CHECK(rvd.get_mem_u8 (base + 5 + offset) == 0x06);
    CHECK(rvd.get_mem_u8 (base + 6 + offset) == 0x07);
    CHECK(rvd.get_mem_u8 (base + 7 + offset) == 0x08);
  }
  CHECK(rvd.get_abstractcs().CMDER == 0);

  // Test misaligned writes
  for (int offset = 0; offset < 4; offset++) {
    rvd.set_mem_u32(base + 0 + offset, 0x04030201);
    rvd.set_mem_u32(base + 4 + offset, 0x08070605);

    for (int i = 0; i < 8; i++) {
      CHECK(rvd.get_mem_u8(base + i + offset) == i + 1);
    }

    rvd.set_mem_u16(base + 0 + offset, 0x0201);
    rvd.set_mem_u16(base + 2 + offset, 0x0403);
    rvd.set_mem_u16(base + 4 + offset, 0x0605);
    rvd.set_mem_u16(base + 6 + offset, 0x0807);

    for (int i = 0; i < 8; i++) {
      CHECK(rvd.get_mem_u8(base + i + offset) == i + 1);
    }

    rvd.set_mem_u8 (base + 0 + offset, 0x01);
    rvd.set_mem_u8 (base + 1 + offset, 0x02);
    rvd.set_mem_u8 (base + 2 + offset, 0x03);
    rvd.set_mem_u8 (base + 3 + offset, 0x04);
    rvd.set_mem_u8 (base + 4 + offset, 0x05);
    rvd.set_mem_u8 (base + 5 + offset, 0x06);
    rvd.set_mem_u8 (base + 6 + offset, 0x07);
    rvd.set_mem_u8 (base + 7 + offset, 0x08);

    for (int i = 0; i < 8; i++) {
      CHECK(rvd.get_mem_u8(base + i + offset) == i + 1);
    }
  }
  CHECK(rvd.get_abstractcs().CMDER == 0);

  // Test aligned block writes
  for (int size = 4; size <= 8; size += 4) {
    for (int offset = 0; offset <= 8; offset += 4) {
      for (int i = 0; i < 4; i++) {
        rvd.set_mem_u32(base + (4 * i), 0xFFFFFFFF);
      }

      uint8_t buf[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
      rvd.set_block_aligned(base + offset, buf, size);

      for (int i = 0; i < offset; i++)         CHECK(rvd.get_mem_u8(base + i) == 0xFF);
      for (int i = 0; i < size; i++)           CHECK(rvd.get_mem_u8(base + i + offset) == i + 1);
      for (int i = offset + size; i < 16; i++) CHECK(rvd.get_mem_u8(base + i) == 0xFF);
    }
  }
  CHECK(rvd.get_abstractcs().CMDER == 0);

  // Test aligned block reads
  for (int size = 4; size <= 8; size += 4) {
    for (int offset = 0; offset <= 8; offset += 4) {
      for (int i = 0;             i < offset;        i++) rvd.set_mem_u8(base + i, 0xFF);
      for (int i = offset;        i < offset + size; i++) rvd.set_mem_u8(base + i, i + 1);
      for (int i = offset + size; i < 16;            i++) rvd.set_mem_u8(base + i, 0xFF);

      uint8_t buf[16];
      memset(buf, 0xFF, sizeof(buf));

      rvd.get_block_aligned(base + offset, buf + 4, size);

      for (int i = 0;        i < 4;        i++) CHECK(buf[i] == 0xFF);
      for (int i = 4;        i < 4 + size; i++) CHECK(buf[i] == i + offset - 3);
      for (int i = 4 + size; i < 16;       i++) CHECK(buf[i] == 0xFF);
    }
  }
  CHECK(rvd.get_abstractcs().CMDER == 0);

  // Test block writes at both ends of memory
  {
    uint32_t block[4] = { 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF };
    CHECK(rvd.get_abstractcs().CMDER == 0);
    rvd.set_block_aligned(0x20000000, block, 16);
    CHECK(rvd.get_abstractcs().CMDER == 0);
  }

  {
    uint32_t block[4] = { 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF };
    CHECK(rvd.get_abstractcs().CMDER == 0);
    rvd.set_block_aligned(0x20000800 - 16, block, 16);
    CHECK(rvd.get_abstractcs().CMDER == 0);
  }

  // Off the high end by 4
  {
    uint32_t block[4] = { 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF };
    CHECK(rvd.get_abstractcs().CMDER == 0);
    rvd.set_block_aligned(0x20000800 - 12, block, 16);
    CHECK(rvd.get_abstractcs().CMDER == 3);
    rvd.clear_err();
  }

  // Off the low end by 4
  {
    uint32_t block[4] = { 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF };
    CHECK(rvd.get_abstractcs().CMDER == 0);
    rvd.set_block_aligned(0x20000000 - 4, block, 16);
    CHECK(rvd.get_abstractcs().CMDER == 3);
    rvd.clear_err();
  }

  printf("All tests pass!\n");
}

//------------------------------------------------------------------------------
