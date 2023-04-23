#include "Console.h"

#include "utils.h"
#include "RVDebug.h"
#include "WCHFlash.h"
#include "SoftBreak.h"
#include "test/tests.h"
#include "example/blink.h"

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

  {
    "write_flash",
    [](Console& c) {
      const uint32_t base = 0x00000000;
      int len = blink_bin_len;
      //int len = 4;
      c.flash->write_flash(base, blink_bin, len);

      uint8_t* readback = new uint8_t[len];

      c.rvd->get_block_aligned(base, readback, len);

      for (int i = 0; i < len; i++) {
        if (blink_bin[i] != readback[i]) {
          LOG_R("Flash readback failed at address 0x%08x - want 0x%02x, got 0x%02x\n", base + i, blink_bin[i], readback[i]);
        }
      }

      delete [] readback;
    }
  },

#if 0
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
#endif
};

static const int handler_count = sizeof(handlers) / sizeof(handlers[0]);

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
