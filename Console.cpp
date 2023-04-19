#include "Console.h"
#include "utils.h"
#include "SLDebugger.h"
#include "pico/stdlib.h"
#include "tests.h"
#include "blink.h"
#include <functional>

//------------------------------------------------------------------------------

void Console::init(SLDebugger* sl) {
  this->sl = sl;
  printf("\n>> ");
}

//------------------------------------------------------------------------------

struct ConsoleHandler {
  const char* name;
  std::function<void(Console&)> handler;
};

ConsoleHandler handlers[] = {
  //{ "run_tests",     [](Console& c) { run_tests(*c.sl);       } },

  { "status",        [](Console& c) { c.sl->status();         } },
  { "reset_dbg",     [](Console& c) { c.sl->reset_dbg();      } },
  { "reset_cpu",     [](Console& c) { c.sl->reset_cpu();      } },
  { "clear_err",     [](Console& c) { c.sl->wire.clear_err(); } },

  { "resume",        [](Console& c) { c.sl->resume();         } },
  { "step",          [](Console& c) { c.sl->step();           } },
  { "halt",          [](Console& c) { c.sl->halt(); printf("Halted at DPC = 0x%08x\n", c.sl->wire.get_csr(CSR_DPC)); } },

  { "lock_flash",    [](Console& c) { c.sl->lock_flash();     } },
  { "unlock_flash",  [](Console& c) { c.sl->unlock_flash();   } },
  { "wipe_chip",     [](Console& c) { c.sl->wipe_chip();      } },
  { "write_flash",   [](Console& c) { c.sl->write_flash(0x00000000, blink_bin, blink_bin_len); } },

  {
    "dump",
    [](Console& c) {
      auto addr = c.packet.take_int().ok_or(0x08000000);

      if (addr & 3) {
        printf("dump - bad addr 0x%08x\n", addr);
      }
      else {
        uint32_t buf[512];
        c.sl->wire.get_block_aligned(addr, buf, 2048);
        for (int y = 0; y < 64; y++) {
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
      if (addr.is_ok()) c.sl->set_breakpoint(addr, 2);
    }
  },

  {
    "clear_bp",
    [](Console& c) {
      auto addr = c.packet.take_int();
      if (addr.is_ok()) c.sl->clear_breakpoint(addr, 2);
    }
  },

  {
    "dump_bp",
    [](Console& c) {
      c.sl->dump_breakpoints();
    }
  },

  {
    "patch_flash",
    [](Console& c) {
      c.sl->patch_flash();
    }
  },

  {
    "unpatch_flash",
    [](Console& c) {
      c.sl->unpatch_flash();
    }
  },
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
