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
  printf("\n>> ");
}

//------------------------------------------------------------------------------

struct ConsoleHandler {
  const char* name;
  std::function<void(Console&)> handler;
};

ConsoleHandler handlers[] = {
  //{ "run_tests",     [](Console& c) { run_tests(*c.sl);       } },

  { "status",        [](Console& c) { c.rvd->dump();         } },
  //{ "reset_dbg",     [](Console& c) { c.rvd->reset_dbg();      } },
  { "reset_cpu",     [](Console& c) { c.rvd->reset_cpu();      } },

  { "resume",        [](Console& c) { c.soft->resume();         } },
  { "step",          [](Console& c) { c.soft->step();           } },
  { "halt",          [](Console& c) { c.soft->halt(); printf("Halted at DPC = 0x%08x\n", c.rvd->get_dpc()); } },

  { "lock_flash",    [](Console& c) { c.flash->lock_flash();     } },
  { "unlock_flash",  [](Console& c) { c.flash->unlock_flash();   } },
  { "wipe_chip",     [](Console& c) { c.flash->wipe_chip();      } },
  { "write_flash",   [](Console& c) { c.flash->write_flash(0x00000000, blink_bin, blink_bin_len); } },

  {
    "dump_addr",
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

  {
    "dump_bp",
    [](Console& c) {
      c.soft->dump();
    }
  },

  /*
  {
    "patch_flash",
    [](Console& c) {
      c.soft->patch_flash();
    }
  },

  {
    "unpatch_flash",
    [](Console& c) {
      c.soft->unpatch_flash();
    }
  },
  */
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
  printf("Command %s not handled\n", packet.buf);
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
