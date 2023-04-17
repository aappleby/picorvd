#include "Console.h"
#include "utils.h"
#include "SLDebugger.h"
#include "WCH_Regs.h"
#include "pico/stdlib.h"
#include "tests.h"
#include "blink.h"

//------------------------------------------------------------------------------

Console::Console() {
}

void Console::init(SLDebugger* sl) {
  this->sl = sl;
  printf("\n>> ");
}

//------------------------------------------------------------------------------

void Console::dispatch_command() {
  //CHECK(sl->sanity());
  auto wire = &sl->wire;

  bool handled = false;

  if (packet.match_word("attach")) {
    sl->attach();
    handled = true;
  }

  if (packet.match_word("detach")) {
    sl->detach();
    handled = true;
  }

  if (packet.match_word("status"))             { handled = true; sl->status(); }
  if (packet.match_word("wipe_chip"))          { handled = true; sl->wipe_chip(); }

  if (packet.match_word("write_flash")) {
    sl->write_flash2(0x00000000, blink_bin, blink_bin_len);
    handled = true;
  }

  if (packet.match_word("resume")) {
    sl->resume2();
    handled = true;
  }

  if (packet.match_word("halt")) {
    sl->halt();
    printf("Halted at DPC = 0x%08x\n", wire->get_csr(CSR_DPC));
    handled = true; 
  }

  if (packet.match_word("reset_dbg"))          { handled = true; sl->reset_dbg(); }
  if (packet.match_word("reset_cpu_and_halt")) { handled = true; sl->reset_cpu_and_halt(); }
  if (packet.match_word("clear_err"))          { handled = true; wire->clear_err(); }
  if (packet.match_word("lock_flash"))         { handled = true; sl->lock_flash(); }
  if (packet.match_word("unlock_flash"))       { handled = true; sl->unlock_flash(); }

  if (packet.match_word("patch_flash"))        { handled = true; sl->patch_flash(); }
  if (packet.match_word("unpatch_flash"))      { handled = true; sl->unpatch_flash(); }

  if (packet.match_word("dump")) {
    auto addr = packet.take_int().ok_or(0x08000000);

    if (addr & 3) {
      printf("dump - bad addr 0x%08x\n", addr);
    }
    else {
      uint32_t buf[512];
      wire->get_block_aligned(addr, buf, 2048);
      for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 8; x++) {
          printf("0x%08x ", buf[x + 8*y]);
        }
        printf("\n");
      }
    }
    handled = true;
  }

  if (packet.match_word("step")) {
    printf("DPC before step 0x%08x\n", wire->get_csr(CSR_DPC));
    sl->step();
    printf("DPC after step  0x%08x\n", wire->get_csr(CSR_DPC));
    handled = true;
  }

  if (packet.match_word("set_bp")) {
    auto addr = packet.take_int();
    if (addr.is_ok()) {
      uint32_t op = wire->get_mem_u32_unaligned(addr);
      printf("bp op 0x%08x\n", op);
      auto size = ((op & 3) == 3) ? 4 : 2;
      sl->set_breakpoint(addr, size);
    }
    else {
      printf("Breakpoint address missing\n");
    }
    handled = true;
  }

  if (packet.match_word("clear_bp")) {
    auto addr = packet.take_int();
    if (addr.is_ok()) {
      uint32_t op = wire->get_mem_u32_unaligned(addr);
      printf("bp op 0x%08x\n", op);
      auto size = ((op & 3) == 3) ? 4 : 2;
      sl->clear_breakpoint(addr, size);
    }
    else {
      printf("Breakpoint address missing\n");
    }
    handled = true;
  }

  if (packet.match_word("dump_bp")) {
    sl->dump_breakpoints();
    handled = true;
  }


  if (!handled) {
    printf("Command %s not handled\n", packet.buf);
  }

  //CHECK(sl->sanity());
}

//------------------------------------------------------------------------------

void Console::update(bool ser_ie, char ser_in) {
  if (ser_ie) {
    if (ser_in == 8) {
      printf("\b \b");
      *packet.cursor2 = 0;
      if (packet.cursor2 > packet.buf) {
        packet.cursor2--;
        packet.size--;
      }
    }
    else if (ser_in == '\n' || ser_in == '\r') {
      *packet.cursor2 = 0;
      packet.cursor2 = packet.buf;

      printf("\n");
      uint32_t time_a = time_us_32();

      //printf("Console packet {%s}\n", packet.buf);
      //printf("%d\n", (packet.cursor2 - packet.buf));
      //printf("%d\n", packet.size);

      dispatch_command();
      uint32_t time_b = time_us_32();

      printf("Command took %d us\n", time_b - time_a);
      if ((packet.cursor2 - packet.buf) < packet.size) {
        printf("Leftover text in packet - {%s}\n", packet.cursor2);
        printf("%d\n", (packet.cursor2 - packet.buf));
        printf("%d\n", packet.size);
      }

      packet.clear();
      printf(">> ");
    }
    else {
      printf("%c", ser_in);
      packet.put(ser_in);
    }
  }
}

//------------------------------------------------------------------------------
