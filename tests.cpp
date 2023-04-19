#include "SLDebugger.h"
#include "string.h"

void run_tests(OneWire& wire) {
  printf("Running tests...\n");

  wire.halt();
  
  uint32_t base = 0x20000400;

  // Test misaligned reads
  for (int offset = 0; offset < 4; offset++) {
    for (int i = 0; i < 8; i++) {
      wire.set_mem_u8(base + i + offset, i + 1);
    }

    CHECK(wire.get_mem_u32(base + 0 + offset) == 0x04030201);
    CHECK(wire.get_mem_u32(base + 4 + offset) == 0x08070605);

    CHECK(wire.get_mem_u16(base + 0 + offset) == 0x0201);
    CHECK(wire.get_mem_u16(base + 2 + offset) == 0x0403);
    CHECK(wire.get_mem_u16(base + 4 + offset) == 0x0605);
    CHECK(wire.get_mem_u16(base + 6 + offset) == 0x0807);

    CHECK(wire.get_mem_u8 (base + 0 + offset) == 0x01);
    CHECK(wire.get_mem_u8 (base + 1 + offset) == 0x02);
    CHECK(wire.get_mem_u8 (base + 2 + offset) == 0x03);
    CHECK(wire.get_mem_u8 (base + 3 + offset) == 0x04);
    CHECK(wire.get_mem_u8 (base + 4 + offset) == 0x05);
    CHECK(wire.get_mem_u8 (base + 5 + offset) == 0x06);
    CHECK(wire.get_mem_u8 (base + 6 + offset) == 0x07);
    CHECK(wire.get_mem_u8 (base + 7 + offset) == 0x08);
  }

  // Test misaligned writes
  for (int offset = 0; offset < 4; offset++) {
    wire.set_mem_u32(base + 0 + offset, 0x04030201);
    wire.set_mem_u32(base + 4 + offset, 0x08070605);

    for (int i = 0; i < 8; i++) {
      CHECK(wire.get_mem_u8(base + i + offset) == i + 1);
    }

    wire.set_mem_u16(base + 0 + offset, 0x0201);
    wire.set_mem_u16(base + 2 + offset, 0x0403);
    wire.set_mem_u16(base + 4 + offset, 0x0605);
    wire.set_mem_u16(base + 6 + offset, 0x0807);

    for (int i = 0; i < 8; i++) {
      CHECK(wire.get_mem_u8(base + i + offset) == i + 1);
    }

    wire.set_mem_u8 (base + 0 + offset, 0x01);
    wire.set_mem_u8 (base + 1 + offset, 0x02);
    wire.set_mem_u8 (base + 2 + offset, 0x03);
    wire.set_mem_u8 (base + 3 + offset, 0x04);
    wire.set_mem_u8 (base + 4 + offset, 0x05);
    wire.set_mem_u8 (base + 5 + offset, 0x06);
    wire.set_mem_u8 (base + 6 + offset, 0x07);
    wire.set_mem_u8 (base + 7 + offset, 0x08);

    for (int i = 0; i < 8; i++) {
      CHECK(wire.get_mem_u8(base + i + offset) == i + 1);
    }
  }

  // Test aligned block writes
  for (int size = 4; size <= 8; size += 4) {
    for (int offset = 0; offset <= 8; offset += 4) {
      for (int i = 0; i < 4; i++) {
        wire.set_mem_u32(base + (4 * i), 0xFFFFFFFF);
      }

      uint8_t buf[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
      wire.set_block_aligned(base + offset, buf, size);

      for (int i = 0; i < offset; i++)         CHECK(wire.get_mem_u8(base + i) == 0xFF);
      for (int i = 0; i < size; i++)           CHECK(wire.get_mem_u8(base + i + offset) == i + 1);
      for (int i = offset + size; i < 16; i++) CHECK(wire.get_mem_u8(base + i) == 0xFF);
    }
  }

  // Test unaligned block writes
  for (int size = 1; size <= 8; size++) {
    for (int offset = 0; offset <= 8; offset++) {
      for (int i = 0; i < 4; i++) {
        wire.set_mem_u32(base + (4 * i), 0xFFFFFFFF);
      }

      uint8_t buf[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
      wire.set_block_unaligned(base + offset, buf, size);
      for (int i = 0; i < offset; i++)         CHECK(wire.get_mem_u8(base + i) == 0xFF);
      for (int i = 0; i < size; i++)           CHECK(wire.get_mem_u8(base + i + offset) == i + 1);
      for (int i = offset + size; i < 16; i++) CHECK(wire.get_mem_u8(base + i) == 0xFF);
    }
  }

  // Test aligned block reads
  for (int size = 4; size <= 8; size += 4) {
    for (int offset = 0; offset <= 8; offset += 4) {
      for (int i = 0;             i < offset;        i++) wire.set_mem_u8(base + i, 0xFF);
      for (int i = offset;        i < offset + size; i++) wire.set_mem_u8(base + i, i + 1);
      for (int i = offset + size; i < 16;            i++) wire.set_mem_u8(base + i, 0xFF);

      uint8_t buf[16];
      memset(buf, 0xFF, sizeof(buf));

      wire.get_block_aligned(base + offset, buf + 4, size);

      for (int i = 0;        i < 4;        i++) CHECK(buf[i] == 0xFF);
      for (int i = 4;        i < 4 + size; i++) CHECK(buf[i] == i + offset - 3);
      for (int i = 4 + size; i < 16;       i++) CHECK(buf[i] == 0xFF);
    }
  }

  // Test unaligned block reads
  for (int size = 1; size <= 8; size++) {
    for (int offset = 0; offset <= 8; offset++) {
      for (int i = 0;             i < offset;        i++) wire.set_mem_u8(base + i, 0xFF);
      for (int i = offset;        i < offset + size; i++) wire.set_mem_u8(base + i, i + 1);
      for (int i = offset + size; i < 16;            i++) wire.set_mem_u8(base + i, 0xFF);

      uint8_t buf[16];
      memset(buf, 0xFF, sizeof(buf));

      wire.get_block_unaligned(base + offset, buf + 4, size);

      for (int i = 0;        i < 4;        i++) CHECK(buf[i] == 0xFF);
      for (int i = 4;        i < 4 + size; i++) CHECK(buf[i] == i + offset - 3);
      for (int i = 4 + size; i < 16;       i++) CHECK(buf[i] == 0xFF);
    }
  }

  printf("All tests pass!\n");
}

//------------------------------------------------------------------------------
