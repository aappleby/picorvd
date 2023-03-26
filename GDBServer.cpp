#include "GDBServer.h"
#include "utils.h"
#include "WCH_Regs.h"
#include <ctype.h>

const char* memory_map = R"(<?xml version="1.0"?>
<!DOCTYPE memory-map PUBLIC "+//IDN gnu.org//DTD GDB Memory Map V1.0//EN" "http://sourceware.org/gdb/gdb-memory-map.dtd">
<memory-map>
  <memory type="flash" start="0x00000000" length="0x4000">
    <property name="blocksize">64</property>
  </memory>
  <memory type="ram" start="0x20000000" length="0x800"/>
</memory-map>
)";

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

/*
‘vFlashErase:addr,length’
‘vFlashWrite:addr:XX...’
‘vFlashDone’
*/

const GDBServer::handler GDBServer::handler_tab[] = {
  { "?",               &GDBServer::handle_questionmark },
  { "!",               &GDBServer::handle_bang },
  { "\003",            &GDBServer::handle_ctrlc },
  { "c",               &GDBServer::handle_c },
  { "D",               &GDBServer::handle_D },
  { "g",               &GDBServer::handle_g },
  { "G",               &GDBServer::handle_G },
  { "H",               &GDBServer::handle_H },
  { "k",               &GDBServer::handle_k },
  { "m",               &GDBServer::handle_m },
  { "M",               &GDBServer::handle_M },
  { "p",               &GDBServer::handle_p },
  { "P",               &GDBServer::handle_P },
  { "q",               &GDBServer::handle_q },
  { "s",               &GDBServer::handle_s },
  { "R",               &GDBServer::handle_R },
  { "v",               &GDBServer::handle_v },
};

const int GDBServer::handler_count = sizeof(GDBServer::handler_tab) / sizeof(GDBServer::handler_tab[0]);

//------------------------------------------------------------------------------

void GDBServer::put_byte(char b) {
  //send_checksum += b;
  cb_put(b);

  if (!sending) {
    log("\n<< ");
    sending = true;
  }

  if (isprint(b)) {
    log("%c", b);
  }
  else {
    log("_");
  }
}

char GDBServer::get_byte() {
  auto b = cb_get();
  //recv_checksum += b;

  if (sending) {
    log("\n>> ");
    sending = false;
  }

  if (isprint(b)) {
    log("%c", b);
  }
  else {
    log("_");
  }

  return b;
}

//------------------------------------------------------------------------------
// Report why the CPU halted

void GDBServer::handle_questionmark() {
  //  SIGINT = 2
  recv.take('?');
  send.set_packet("T02");
}

//------------------------------------------------------------------------------
// Enable extended mode

void GDBServer::handle_bang() {
  recv.take('!');
  send.set_packet("OK");
}

//------------------------------------------------------------------------------
// Break

void GDBServer::handle_ctrlc() {
  send.set_packet("OK");
}

//------------------------------------------------------------------------------
// Continue - "c<addr>"

void GDBServer::handle_c() {
  recv.take('c');
  send.set_packet("");
}

//------------------------------------------------------------------------------

void GDBServer::handle_D() {
  recv.take('D');
  send.set_packet("OK");
}

//------------------------------------------------------------------------------
// Read general registers

void GDBServer::handle_g() {
  recv.take('g');

  if (!recv.error) {
    send.start_packet();
    for (int i = 0; i < 16; i++) {
      send.put_u32(sl->get_gpr(i));
    }
    send.put_u32(sl->get_csr(CSR_DPC));
    send.end_packet();
  }
}

//----------
// Write general registers

void GDBServer::handle_G() {
  recv.take('G');
  /*
  packet_start();
  for (int i = 0; i < 32; i++) {
    packet_u32(0xF00D0000 + i);
  }
  packet_u32(0x00400020); // main() in basic
  packet_end();
  */

  send.set_packet("");
}

//------------------------------------------------------------------------------
// Set thread for subsequent operations

void GDBServer::handle_H() {
  recv.take('H');
  recv.skip(1);
  int thread_id = recv.take_i32();

  if (recv.error) {
    send.set_packet("E01");
  }
  else {
    send.set_packet("OK");
  }
}

//------------------------------------------------------------------------------
// Kill

void GDBServer::handle_k() {
  recv.take('k');
  // 'k' always kills the target and explicitly does _not_ have a reply.
}

//------------------------------------------------------------------------------
// Read memory

void GDBServer::handle_m() {
  recv.take('m');
  int addr = recv.take_i32();
  recv.take(',');
  int size = recv.take_i32();

  if (recv.error) {
    log("\nhandle_m %x %x - recv.error '%s'\n", addr, size, recv.buf);
    send.set_packet("");
    return;
  }

  if (size == 4) {
    send.start_packet();
    send.put_u32(sl->get_mem(addr));
    send.end_packet();
  }
  else {
    uint32_t buf[256];
    sl->get_block(addr, buf, size/4);

    send.start_packet();
    for (int i = 0; i < size/4; i++) {
      send.put_u32(buf[i]);
    }
    send.end_packet();
  }
}

//------------------------------------------------------------------------------
// Write memory

// GDB also uses this for flash write?

void GDBServer::handle_M() {
  recv.take('M');
  int addr = recv.take_i32();
  recv.take(',');
  int len = recv.take_i32();
  recv.take(':');

  // FIXME handle non-aligned, non-dword writes?
  if (recv.error || (len % 4) || (addr % 4)) {
    send.set_packet("");
    return;
  }

  for (int i = 0; i < len/4; i++) {
    sl->put_mem(addr, recv.take_i32());
    addr += 4;
  }

  if (recv.error) {
    send.set_packet("");
  }
  else {
    send.set_packet("OK");
  }
}

//------------------------------------------------------------------------------
// Read the value of register N

void GDBServer::handle_p() {
  recv.take('p');
  int gpr = recv.take_i32();

  if (!recv.error) {
    send.start_packet();
    send.put_u32(sl->get_gpr(gpr));
    send.end_packet();
  }
}

//----------
// Write the value of register N

void GDBServer::handle_P() {
  recv.take('P');
  int gpr = recv.take_i32();
  recv.take('=');
  unsigned int val = recv.take_u32();

  if (!recv.error) {
    sl->put_gpr(gpr, val);
    send.set_packet("OK");
  }
}

//------------------------------------------------------------------------------

void GDBServer::handle_q() {
  if (recv.match("qAttached")) {
    // Query what GDB is attached to
    // Reply:
    // ‘1’ The remote server attached to an existing process.
    // ‘0’ The remote server created a new process.
    // ‘E NN’ A badly formed request or an error was encountered.
    send.set_packet("1");
  }
  else if (recv.match("qC")) {
    // -> qC
    // Return current thread ID
    // Reply: ‘QC<thread-id>’
    send.set_packet("QC0");
  }
  else if (recv.match("qfThreadInfo")) {
    // Query all active thread IDs
    send.set_packet("m0");
  }
  else if (recv.match("qsThreadInfo")) {
    // Query all active thread IDs, continued
    send.set_packet("l");
  }
  else if (recv.match("qSupported")) {
    // FIXME we're ignoring the contents of qSupported
    recv.cursor = recv.size;
    send.set_packet("PacketSize=32768;qXfer:memory-map:read+");
  }
  else if (recv.match("qXfer:")) {
    if (recv.match("memory-map:read::")) {
      int offset = recv.take_i32();
      recv.take(',');
      int length = recv.take_i32();

      if (recv.error) {
        send.set_packet("E00");
      }
      else {
        send.start_packet();
        send.put_str("l");
        send.put_str(memory_map);
        send.end_packet();
      }
    }

    // FIXME handle other xfer packets
  }
  else {
    // Unknown query, ignore it
    recv.cursor = recv.size;
    send.set_packet("");
  }
}

//------------------------------------------------------------------------------
// Restart

void GDBServer::handle_R() {
  recv.take('R');
  send.set_packet("");
}

//------------------------------------------------------------------------------
// Step

void GDBServer::handle_s() {
  recv.take('s');
  send.set_packet("");
}

//------------------------------------------------------------------------------

void GDBServer::handle_v() {
  if (recv.match("vCont")) {
    send.set_packet("");
  }
  else if (recv.match("vFlash")) {
    if (recv.match("Write")) {
      recv.take(':');
      int addr = recv.take_i32();
      recv.take(":");
      log("\n");
      log("recv.size   %d\n", recv.size);
      log("recv.cursor %d\n", recv.cursor);
      int size = recv.size - recv.cursor;
      log("Write size %d, write addr %x\n", size, addr);
      recv.cursor = recv.size;
      send.set_packet("OK");
    }
    else if (recv.match("Erase")) {
      recv.take(':');
      int addr = recv.take_i32();
      recv.take(',');
      int size = recv.take_i32();

      if (recv.error) {
        send.set_packet("E00");
      }
      else {
        send.set_packet("OK");
      }
    }
    else if (recv.match("Done")) {
      send.set_packet("OK");
    }
  }
  else if (recv.match("vKill")) {
    send.set_packet("OK");
  }
  else if (recv.match("vMustReplyEmpty")) {
    send.set_packet("");
  }
}

//------------------------------------------------------------------------------

void GDBServer::loop() {
  log("GDBServer::loop()\n");

  while(1) {

    // Wait for start char
    while (get_byte() != '$');

    // Add bytes to packet until we see the end char
    // Checksum is for the _escaped_ data.
    recv.clear();
    uint8_t checksum = 0;
    while (1) {
      char c = get_byte();
      if (c == '#') {
        break;
      }
      else if (c == '}') {
        checksum += c;
        c = get_byte();
        checksum += c;
        recv.put_buf(c ^ 0x20);
      }
      else {
        checksum += c;
        recv.put_buf(c);
      }
    }
    char expected_checksum = 0;
    expected_checksum = (expected_checksum << 4) | from_hex(get_byte());
    expected_checksum = (expected_checksum << 4) | from_hex(get_byte());

    if (checksum != expected_checksum) {
      log("\n");
      log("Packet transmission error\n");
      log("expected checksum 0x%02x\n", expected_checksum);
      log("actual checksum   0x%02x\n", checksum);
      put_byte('-');
      continue;
    }

    // Packet checksum OK, handle it.
    put_byte('+');


    handler_func h = nullptr;
    for (int i = 0; i < handler_count; i++) {
      if (cmp(handler_tab[i].name, recv.buf) == 0) {
        h = handler_tab[i].handler;
      }
    }

    if (h) {
      recv.cursor = 0;
      send.clear();
      (*this.*h)();

      if (recv.error) {
        log("\nParse failure for packet!\n");
        send.set_packet("E00");
      }
      else if (recv.cursor != recv.size) {
        log("\nLeftover text in packet - \"%s\"\n", &recv.buf[recv.cursor]);
      }
    }
    else {
      log("No handler for command %s\n", recv.buf);
      send.set_packet("");
    }

    if (!send.packet_valid) {
      log("Handler created a bad packet '%s'\n", send.buf);
      send.set_packet("");
    }

    for (int retry = 0; retry < 5; retry++) {
      if (send_packet()) break;
      if (retry == 4) {
        log("\nFailed to send packet after 5 retries\n");
      }
    }
  }
}

//------------------------------------------------------------------------------
// The binary data representation uses 7d (ASCII ‘}’) as an escape character.
// Any escaped byte is transmitted as the escape character followed by the
// original character XORed with 0x20. For example, the byte 0x7d would be
// transmitted as the two bytes 0x7d 0x5d. The bytes 0x23 (ASCII ‘#’), 0x24
//(ASCII ‘$’), and 0x7d (ASCII ‘}’) must always be escaped. Responses sent by
// the stub must also escape 0x2a (ASCII ‘*’), so that it is not interpreted
// as the start of a run-length encoded sequence (described next).

bool GDBServer::send_packet() {

  put_byte('$');
  uint8_t checksum = 0;
  for (int i = 0; i < send.size; i++) {
    char c = send.buf[i];
    if (c == '#' || c == '$' || c == '}' || c == '*') {
      checksum += '}';
      put_byte('}');
      checksum += c ^ 0x20;
      put_byte(c ^ 0x20);
    }
    else {
      checksum += c;
      put_byte(c);
    }
  }
  put_byte('#');
  put_byte(to_hex((checksum >> 4) & 0xF));
  put_byte(to_hex((checksum >> 0) & 0xF));

  while(1) {
    char c = get_byte();
    if (c == '+') {
      return true;
    }
    else if (c == '-') {
      log("========================\n");
      log("========  NACK  ========\n");
      log("========================\n");
      return false;
    }
    else {
      // FIXME - are we still seeing this? YES - nulls
      //log("garbage ack char %d '%c'\n", c, c);
      //return false;
    }
  }
}

//------------------------------------------------------------------------------
