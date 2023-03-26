#include "GDBServer.h"
#include "utils.h"
#include "WCH_Regs.h"

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
  cb_put(b);

  if (!sending) {
    log("\n<< ");
    sending = true;
  }

  if (b) {
    log("%c", b);
  } else {
    log("\0");
  }
}

char GDBServer::get_byte() {
  auto b = cb_get();

  if (sending) {
    log("\n>> ");
    sending = false;
  }

  if (b) {
    log("%c", b);
  } else {
    log("\0");
  }

  return b;
}

//------------------------------------------------------------------------------
// Report why the CPU halted

void GDBServer::handle_questionmark() {
  //  SIGINT = 2
  send.set_packet("T02");
}

//------------------------------------------------------------------------------
// Enable extended mode

void GDBServer::handle_bang() {
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
  send.set_packet("");
}

//------------------------------------------------------------------------------

void GDBServer::handle_D() {
  send.set_packet("OK");
}

//------------------------------------------------------------------------------
// Read general registers

void GDBServer::handle_g() {
  send.start_packet();
  for (int i = 0; i < 16; i++) {
    send.put_u32(sl->get_gpr(i));
  }
  send.put_u32(sl->get_csr(CSR_DPC));
  send.end_packet();
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

  send.set_packet("");
}

//------------------------------------------------------------------------------
// Set thread for subsequent operations

void GDBServer::handle_H() {
  recv.take('H');
  recv.skip(1);
  int thread_id = recv.take_hex();

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
  // 'k' always kills the target and explicitly does _not_ have a reply.
}

//------------------------------------------------------------------------------
// Read memory

void GDBServer::handle_m() {
  recv.take('m');
  int addr = recv.take_hex();
  recv.take(',');
  int size = recv.take_hex();

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
  int addr = recv.take_hex();
  recv.take(',');
  int len = recv.take_hex();
  recv.take(':');

  // FIXME handle non-aligned, non-dword writes?
  if (recv.error || (len % 4) || (addr % 4)) {
    send.set_packet("");
    return;
  }

  for (int i = 0; i < len/4; i++) {
    sl->put_mem(addr, recv.take_hex());
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
  int gpr = recv.take_hex();

  if (!recv.error) {
    send.start_packet();
    send.put_u32(sl->get_gpr(gpr));
    send.end_packet();
  }
}

//----------
// Write the value of register N

void GDBServer::handle_P() {
  send.set_packet("");
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
  else if (recv.match("qL")) {
    // Query thread information from RTOS
    // FIXME get arg
    // qL1200000000000000000
    // qL 1 20 00000000 00000000
    // not sure if this is correct.
    send.set_packet("qM 01 1 00000000 00000001");
  }
  else if (recv.match("qOffsets")) {
    // Get section offsets that the target used when relocating the downloaded
    // image. We don't have any offsets.
    send.set_packet("");
  }
  else if (recv.match("qTStatus")) {
    // Query trace status
    send.set_packet("T0");
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
    send.set_packet("PacketSize=32768;qXfer:memory-map:read+");
  }
  else if (recv.match("qXfer:")) {
    if (recv.match("memory-map:read::")) {
      int offset = recv.take_hex();
      recv.take(',');
      int length = recv.take_hex();

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
    send.set_packet("");
  }
}

//------------------------------------------------------------------------------
// Restart

void GDBServer::handle_R() {
  send.set_packet("");
}

//------------------------------------------------------------------------------
// Step

void GDBServer::handle_s() {
  send.set_packet("");
}

//------------------------------------------------------------------------------

void GDBServer::handle_v() {
  if (recv.match("vCont")) {
    send.set_packet("");
  }
  else if (recv.match("vFlash")) {
    if (recv.match("Erase")) {
      recv.take(':');
      int start = recv.take_hex();
      recv.take(',');
      int size = recv.take_hex();

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
    recv.clear();
    while (1) {
      char c = get_byte();
      if (c == '#') {
        break;
      }
      else if (c == '}') {
        recv.put_buf(get_byte() ^ 0x20);
      }
      else {
        recv.put_buf(c);
      }
    }
    char expected_checksum = 0;
    expected_checksum = (expected_checksum << 4) | from_hex(get_byte());
    expected_checksum = (expected_checksum << 4) | from_hex(get_byte());

    if (recv.checksum != expected_checksum) {
      log("\n");
      log("Packet transmission error\n");
      log("expected checksum 0x%02x\n", expected_checksum);
      log("actual checksum   0x%02x\n", recv.checksum);
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
  for (int i = 0; i < send.size; i++) {
    char c = send.buf[i];
    if (c == '#' || c == '$' || c == '}' || c == '*') {
      put_byte('}');
      put_byte(c ^ 0x20);
    }
    else {
      put_byte(c);
    }
  }
  put_byte('#');
  put_byte(to_hex((send.checksum >> 4) & 0xF));
  put_byte(to_hex((send.checksum >> 0) & 0xF));

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
