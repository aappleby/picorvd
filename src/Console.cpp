#include "Console.h"

#include "utils.h"
#include "RVDebug.h"
#include "WCHFlash.h"
#include "SoftBreak.h"
#include "test/tests.h"
#ifdef INCLUDE_BLINKY_BINARY
#include "example/bin/blink.h"
#endif

#include <functional>
#include "pico/stdlib.h"

//------------------------------------------------------------------------------

Console::Console(RVDebug* rvd, WCHFlash* flash, SoftBreak* soft) {
  this->rvd = rvd;
  this->flash = flash;
  this->soft = soft;
}

void Console::reset() {
}

void Console::dump() {
}

void Console::start() {
  printf_y("\n>> ");
}

//------------------------------------------------------------------------------

struct ConsoleHandler {
  const char* name;
  std::function<void(Console&)> handler;
};

ConsoleHandler handlers[] = {
  {
    "help",
    [](Console& c) {
      c.print_help();
    }
  },

  { "run_tests", [](Console& c) { run_tests(*c.rvd); } },

  {
    "status",
    [](Console& c) {
      c.rvd->dump();
    }
  },

  {
    "reset",
    [](Console& c) {
      if (c.rvd->reset()) {
        printf_g("Reset OK\n");
      }
      else {
        printf_r("Reset failed\n");
      }
    }
  },

  {
    "halt",
    [](Console& c) {
      if (c.rvd->halt()) {
        printf_g("Halted at DPC = 0x%08x\n", c.rvd->get_dpc());
      }
      else {
        printf_r("Halt failed\n");
      }
    }
  },

  {
    "resume",
    [](Console& c) {
      if (c.rvd->resume()) {
        printf_g("Resume OK\n");
      }
      else {
        printf_g("Resume failed\n");
      }
    }
  },

  {
    "step",
    [](Console& c) {
      if (c.rvd->step()) {
        printf_g("Stepped to DPC = 0x%08x\n", c.rvd->get_dpc());
      }
      else {
        printf_r("Step failed\n");
      }
    }
  },

  {
    "dump",
    [](Console& c) {
      auto addr = c.packet.take_int().ok_or(0x08000000);
      printf("addr 0x%08x\n", addr);

      if (addr & 3) {
        printf("dump - bad addr 0x%08x\n", addr);
      }
      else {
        uint32_t buf[24*8];
        c.rvd->get_block_aligned(addr, buf, 24*8*4);
        for (int y = 0; y < 24; y++) {
          for (int x = 0; x < 8; x++) {
            printf("0x%08x ", buf[x + 8*y]);
          }
          printf("\n");
        }
      }
    }
  },

  { "lock_flash",    [](Console& c) { c.flash->lock_flash();     } },
  { "unlock_flash",  [](Console& c) { c.flash->unlock_flash();   } },
  { "wipe_chip",     [](Console& c) { c.flash->wipe_chip();      } },
  { "flash_status",  [](Console& c) { c.flash->dump(); } },
#ifdef INCLUDE_BLINKY_BINARY
  {
    "write_flash",
    [](Console& c) {
      const uint32_t base = 0x00000000;
      int len = blink_bin_len;
      printf_b("Writing flash - %d bytes\n", len);
      uint32_t time_a = time_us_32();
      c.flash->write_flash(base, blink_bin, len);
      uint32_t time_b = time_us_32();
      printf_b("Done in %d usec, %f bytes/sec\n", time_b - time_a,  1000000.0 * float(len) / float(time_b - time_a));
    }
  },

  {
    "check_flash",
    [](Console& c) {
      const uint32_t base = 0x00000000;
      int len = blink_bin_len;
      printf_b("Verifying flash - %d bytes\n", len);
      uint32_t time_a = time_us_32();
      c.flash->verify_flash(base, blink_bin, len);
      uint32_t time_b = time_us_32();
      printf_b("Done in %d usec, %f bytes/sec\n", time_b - time_a,  1000000.0 * float(len) / float(time_b - time_a));
    }
  },
#endif
  { "soft_halt",   [](Console& c) { c.soft->halt(); printf("Halted at DPC = 0x%08x\n", c.rvd->get_dpc()); } },
  { "soft_resume", [](Console& c) { c.soft->resume();         } },
  { "soft_step",   [](Console& c) { c.soft->step();           } },

  {
    "dump_bp",
    [](Console& c) {
      c.soft->dump();
    }
  },

  {
    "set_bp",
    [](Console& c) {
      auto addr = c.packet.take_int();
      if (addr.is_ok()) c.soft->set_breakpoint(addr, 2);
    }
  },

  {
    "clear_bp",
    [](Console& c) {
      auto addr = c.packet.take_int();
      if (addr.is_ok()) c.soft->clear_breakpoint(addr, 2);
    }
  },

  { "patch_flash",   [](Console& c) { c.soft->patch_flash(); } },
  { "unpatch_flash", [](Console& c) { c.soft->unpatch_flash(); } },
};

static const int handler_count = sizeof(handlers) / sizeof(handlers[0]);

//------------------------------------------------------------------------------

void Console::print_help() {
  printf("Commands:\n");
  for (int i = 0; i < handler_count; i++) {
    printf("  %s\n", handlers[i].name);
  }
}

//------------------------------------------------------------------------------

void Console::dispatch_command() {
  for (int i = 0; i < handler_count; i++) {
    auto& h = handlers[i];
    if (packet.match_word(h.name)) {
      h.handler(*this);
      return;
    }
  }
  printf_r("Command %s not handled\n", packet.buf);
}

//------------------------------------------------------------------------------

void Console::update(bool ser_ie, char ser_in) {
  if (ser_ie) {
    if (ser_in == 8 /*backspace*/) {
      printf("\b \b");
      if (packet.cursor2 > packet.buf) {
        packet.cursor2--;
        packet.size--;
        *packet.cursor2 = 0;
      }
    }
    else if (ser_in == '\n' || ser_in == '\r') {
      *packet.cursor2 = 0;
      packet.cursor2 = packet.buf;

      printf("\n");
      uint32_t time_a = time_us_32();
      dispatch_command();
      uint32_t time_b = time_us_32();

      printf("Command took %d us\n", time_b - time_a);
      if ((packet.cursor2 - packet.buf) < packet.size) {
        printf_r("Leftover text in packet - {%s}\n", packet.cursor2);
      }

      packet.clear();
      printf_y(">> ");
    }
    else {
      printf("%c", ser_in);
      packet.put(ser_in);
    }
  }
}

//------------------------------------------------------------------------------
