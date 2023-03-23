#include "gdb_server.h"
#include "utils.h"

//------------------------------------------------------------------------------
/*
At a minimum, a stub is required to support the ‘?’ command to tell GDB the
reason for halting, ‘g’ and ‘G’ commands for register access, and the ‘m’ and
‘M’ commands for memory access. Stubs that only control single-threaded targets
can implement run control with the ‘c’ (continue) command, and if the target
architecture supports hardware-assisted single-stepping, the ‘s’ (step) command.
Stubs that support multi-threading targets should support the ‘vCont’ command.
All other commands are optional.
*/

const GDBServer::handler GDBServer::handler_tab[] = {
  { "?",               &GDBServer::handle_questionmark },
  { "!",               &GDBServer::handle_bang },
  { "\003",            &GDBServer::handle_ctrlc },
  { "c",               &GDBServer::handle_c },
  { "D",               &GDBServer::handle_D },
  { "g",               &GDBServer::handle_g },
  { "H",               &GDBServer::handle_H },
  { "k",               &GDBServer::handle_k },
  { "m",               &GDBServer::handle_m },
  { "M",               &GDBServer::handle_M },
  { "p",               &GDBServer::handle_p },
  { "P",               &GDBServer::handle_P },
  { "qAttached",       &GDBServer::handle_qAttached },
  { "qC",              &GDBServer::handle_qC },
  { "qfThreadInfo",    &GDBServer::handle_qfThreadInfo },
  { "qL",              &GDBServer::handle_qL },
  { "qOffsets",        &GDBServer::handle_qOffsets },
  { "qsThreadInfo",    &GDBServer::handle_qsThreadInfo },
  { "qSupported",      &GDBServer::handle_qSupported },
  { "qSymbol",         &GDBServer::handle_qSymbol },
  { "qTfP",            &GDBServer::handle_qTfP },
  { "qTfV",            &GDBServer::handle_qTfV },
  { "qTStatus",        &GDBServer::handle_qTStatus },
  { "s",               &GDBServer::handle_s },
  { "vCont?",          &GDBServer::handle_vCont },
  { "vKill",           &GDBServer::handle_vKill },
  { "vMustReplyEmpty", &GDBServer::handle_vMustReplyEmpty },
};

const int GDBServer::handler_count = sizeof(GDBServer::handler_tab) / sizeof(GDBServer::handler_tab[0]);

//------------------------------------------------------------------------------

void GDBServer::put_byte(char b) {
  send_checksum += b;
  cb_put(b);

  if (!sending) {
    log("\n<< ");
    sending = true;
  }

  log("%c", b);
}

char GDBServer::get_byte() {
  auto b = cb_get();

  if (sending) {
    log("\n>> ");
    sending = false;
  }

  log("%c", b);
  recv_checksum += b;
  return b;
}

//------------------------------------------------------------------------------

void GDBServer::packet_start() {
  put_byte('$');
  send_checksum = 0;
}

void GDBServer::packet_data(const char* buf, int len) {
  for (int i = 0; i < len; i++) {
    put_byte(packet[i]);
  }
}

void GDBServer::packet_string(const char* buf) {
  while(*buf) {
    put_byte(*buf++);
  }
}

void GDBServer::packet_u8(char x) {
  put_byte(to_hex((x >> 4) & 0xF));
  put_byte(to_hex((x >> 0) & 0xF));
}

void GDBServer::packet_u32(int x) {
  packet_u8(x >> 0);
  packet_u8(x >> 8);
  packet_u8(x >> 16);
  packet_u8(x >> 24);
}

void GDBServer::packet_end() {
  char old_checksum = send_checksum;
  put_byte('#');
  packet_u8(old_checksum);
}

bool GDBServer::wait_packet_ack() {
  //log("wait_packet_ack()\n");
  char c = 0;
  do { c = get_byte(); }
  while (c != '+' && c != '-');
  /*
  if (c == '-') {
    LOG_R("==============================\n");
    LOG_R("===========  NACK  ===========\n");
    LOG_R("==============================\n");
  }
  */
  //log("wait_packet_ack() done\n");
  return c == '+';
}

//------------------------------------------------------------------------------

void GDBServer::send_packet(const char* packet) {
  while(1) {
    packet_start();
    packet_string(packet);
    packet_end();
    if (wait_packet_ack()) break;
  }
}

void GDBServer::send_ack()     { put_byte('+'); }
void GDBServer::send_nack()    { put_byte('-'); }
void GDBServer::send_ok()      { send_packet("OK"); }
void GDBServer::send_empty()   { send_packet(""); }
void GDBServer::send_nothing() { }

//------------------------------------------------------------------------------
// Report why the CPU halted

void GDBServer::handle_questionmark() {
  //  SIGINT = 2
  send_packet("T02");
}

//----------
// Enable extended mode

void GDBServer::handle_bang() {
  send_ok();
}

//----------
// Break

void GDBServer::handle_ctrlc() {
  send_ok();
}

//----------
// Continue

void GDBServer::handle_c() {
  send_empty();
}

//----------

void GDBServer::handle_D() {
  send_ok();
}

//----------
// Read general registers

void GDBServer::handle_g() {
  packet_start();
  // RV32E registers
  for (int i = 0; i < 16; i++) {
    packet_u32(0xF00D0000 + i);
  }
  packet_u32(0x00400020); // PC
  packet_end();
}

//----------
// Write general registers

void GDBServer::handle_G() {
  /*
  packet_start();
  for (int i = 0; i < 32; i++) {
    packet_u32(0xF00D0000 + i);
  }
  packet_u32(0x00400020); // main() in basic
  packet_end();
  */
}

//----------
// Set thread for subsequent operations

void GDBServer::handle_H() {
  /*
  packet_cursor++;
  int thread_id = _atoi(packet_cursor);
  if (thread_id == 0 || thread_id == 1) {
    send_ok();
  }
  else {
    send_packet("E01");
  }
  */
  send_ok();
}

//----------
// Kill

void GDBServer::handle_k() {
  // 'k' always kills the target and explicitly does _not_ have a reply.
}

//----------
// Read memory

void GDBServer::handle_m() {
  send_empty();
}

//----------
// Write memory

void GDBServer::handle_M() {
  send_empty();
}

//----------
// Read the value of register N

void GDBServer::handle_p() {
  packet_cursor++;
  // FIXME are we sure this is hex? I think it is...
  int hi = from_hex(*packet_cursor++);
  int lo = from_hex(*packet_cursor++);
  packet_start();
  packet_u32(0xF00D0000 + ((hi << 4) | lo));
  packet_end();
}

//----------
// Write the value of register N

void GDBServer::handle_P() {
}

//----------
// Query what GDB is attached to

void GDBServer::handle_qAttached() {
  // -> qAttached
  // Reply:
  // ‘1’ The remote server attached to an existing process.
  // ‘0’ The remote server created a new process.
  // ‘E NN’ A badly formed request or an error was encountered.
  send_packet("1");
}

//----------
// Query thread ID

void GDBServer::handle_qC() {
  // -> qC
  // Return current thread ID
  // Reply: ‘QC thread-id’
  send_packet("QC 1");
}

//----------

void GDBServer::handle_qOffsets() {
  send_empty();
}

//----------
// Query thread information from RTOS

void GDBServer::handle_qL() {
  // FIXME get arg
  // qL1200000000000000000
  // qL 1 20 00000000 00000000
  // not sure if this is correct.
  send_packet("qM 01 1 00000000 00000001");
}

//----------
// Query trace status

void GDBServer::handle_qTStatus() {
  send_packet("T0");
}

//----------
// Query all active thread IDs

void GDBServer::handle_qfThreadInfo() {
  // Only one thread with id 1.
  send_packet("m 1");
}

//----------
// Query all active thread IDs, continued

void GDBServer::handle_qsThreadInfo() {
  // Only one thread with id 1.
  send_packet("l");
}

//----------

void GDBServer::handle_qSupported() {
  // Tell GDB stuff we support (nothing)
  send_empty();
}

//----------

void GDBServer::handle_qSymbol() {
  send_ok();
}

//----------

void GDBServer::handle_qTfP() {
  send_empty();
}

//----------

void GDBServer::handle_qTfV() {
  send_empty();
}

//----------
// Step

void GDBServer::handle_s() {
}

//----------
// Resume, with different actions for each thread

void GDBServer::handle_vCont() {
}

//----------

void GDBServer::handle_vKill() {
  send_ok();
}

//----------

void GDBServer::handle_vMustReplyEmpty() {
  send_empty();
}

//------------------------------------------------------------------------------

void GDBServer::dispatch_command() {
  for (int i = 0; i < handler_count; i++) {
    if (cmp(handler_tab[i].name, packet) == 0) {
      (*this.*handler_tab[i].handler)();
      return;
    }
  }

  log("   No handler for command %s\n", packet);
  send_empty();
}

//------------------------------------------------------------------------------

void GDBServer::loop() {
  log("GDBServer::loop()\n");

  //print_to(cb_put, "GDBServer::loop() cb_put\n");

  while(1) {
    //char c = get_byte();
    //if (c == '$') log("\n");
    //log("%c", c);

    packet_size = 0;
    packet_cursor = packet;
    char c;

    char expected_checksum = 0;
    char actual_checksum = 0;

    while (1) {
      c = get_byte();
      if (c == '$') {
        break;
      }
    }

    //log("saw $\n");

    while (1) {
      c = get_byte();

      //if (c == '$') log("\n");
      //log("%c", c);

      if (c == '#') {
        packet[packet_size] = 0;
        break;
      }
      else {
        actual_checksum += c;
        packet[packet_size++] = c;
      }
    }

    for (int digits = 0; digits < 2; digits++) {
      while (1) {
        c = get_byte();
        int d = from_hex(c);
        if (d == -1) {
          log("bad hex %c\n", c);
          continue;
        }
        else {
          expected_checksum = (expected_checksum << 4) | d;
          break;
        }
      }
    }

    if (actual_checksum != expected_checksum) {
      //log("==>> %s\n", packet);
      log.R("Packet transmission error\n");
      log.R("expected checksum 0x%02x\n", expected_checksum);
      log.R("actual checksum   0x%02x\n", actual_checksum);
      send_nack();
      continue;
    } else {
      //log("==>> %s\n", packet);
      send_ack();
      //log("<<== ");
      dispatch_command();
      //log("\n");
    }
  }
}

//------------------------------------------------------------------------------
