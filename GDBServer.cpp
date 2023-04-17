#include "GDBServer.h"
#include "utils.h"
#include "WCH_Regs.h"
#include <ctype.h>

uint32_t swap(uint32_t x) {
  uint32_t a = (x >>  0) & 0xFF;
  uint32_t b = (x >>  8) & 0xFF;
  uint32_t c = (x >> 16) & 0xFF;
  uint32_t d = (x >> 24) & 0xFF;

  return (a << 24) | (b << 16) | (c << 8) | (d << 0);
}

const char* memory_map = R"(<?xml version="1.0"?>
<!DOCTYPE memory-map PUBLIC "+//IDN gnu.org//DTD GDB Memory Map V1.0//EN" "http://sourceware.org/gdb/gdb-memory-map.dtd">
<memory-map>
  <memory type="flash" start="0x00000000" length="0x4000">
    <property name="blocksize">64</property>
  </memory>
  <memory type="ram" start="0x20000000" length="0x800"/>
</memory-map>
)";

GDBServer::GDBServer() {
  this->page_base = -1;
  this->page_bitmap = 0;
  for (int i = 0; i < 64; i++) this->page_cache[i] = 0xFF;
}

void GDBServer::init(SLDebugger* sl) {
  this->sl = sl;
  this->wire = &sl->wire;
}

//------------------------------------------------------------------------------
/*
At a minimum, a stub is required to support the ‘?’ command to tell GDB the
reason for halting, ‘g’ and ‘G’ commands for register access, and the ‘m’ and
‘M’ commands for memory access. Stubs that only control single-threaded targets
can implement run control with the ‘c’ (continue) command, and if the target
architecture supports hardware-assisted single-stepping, the ‘s’ (step) command.
Stubs that support multi-threading targets should support the ‘vCont’ command.
All other commands are optional.

// The binary data representation uses 7d (ASCII ‘}’) as an escape character.
// Any escaped byte is transmitted as the escape character followed by the
// original character XORed with 0x20. For example, the byte 0x7d would be
// transmitted as the two bytes 0x7d 0x5d. The bytes 0x23 (ASCII ‘#’), 0x24
// (ASCII ‘$’), and 0x7d (ASCII ‘}’) must always be escaped. Responses sent by
// the stub must also escape 0x2a (ASCII ‘*’), so that it is not interpreted
// as the start of a run-length encoded sequence (described next).
*/

const GDBServer::handler GDBServer::handler_tab[] = {
  { "?",     &GDBServer::handle_questionmark },
  { "!",     &GDBServer::handle_bang },
  { "c",     &GDBServer::handle_c },
  { "D",     &GDBServer::handle_D },
  { "g",     &GDBServer::handle_g },
  { "G",     &GDBServer::handle_G },
  { "H",     &GDBServer::handle_H },
  { "k",     &GDBServer::handle_k },
  { "m",     &GDBServer::handle_m },
  { "M",     &GDBServer::handle_M },
  { "p",     &GDBServer::handle_p },
  { "P",     &GDBServer::handle_P },
  { "q",     &GDBServer::handle_q },
  { "s",     &GDBServer::handle_s },
  { "R",     &GDBServer::handle_R },
  { "v",     &GDBServer::handle_v },
  { "z0",    &GDBServer::handle_z0 },
  { "Z0",    &GDBServer::handle_Z0 },
  { "z1",    &GDBServer::handle_z1 },
  { "Z1",    &GDBServer::handle_Z1 },
};

const int GDBServer::handler_count = sizeof(GDBServer::handler_tab) / sizeof(GDBServer::handler_tab[0]);

//------------------------------------------------------------------------------
// Report why the CPU halted

void GDBServer::handle_questionmark() {
  //  SIGINT = 2
  recv.take('?');
  send.set_packet("T05");
}

//------------------------------------------------------------------------------
// Enable extended mode

void GDBServer::handle_bang() {
  recv.take('!');
  send.set_packet("OK");
}

//------------------------------------------------------------------------------
// Continue - "c<addr>"

void GDBServer::handle_c() {
  recv.take('c');

  // Set PC if requested
  uint32_t addr = 0;
  if (recv.maybe_take_hex(addr)) {
    wire->set_csr(CSR_DPC, addr);
  }

  // If we did not actually resume because we immediately hit a breakpoint,
  // respond with a "hit breakpoint" message. Otherwise we do not reply until
  // the hart stops.

  if (!sl->resume2()) {
    send.set_packet("T05");
  }
}

//------------------------------------------------------------------------------

void GDBServer::handle_D() {
  recv.take('D');
  printf("GDB detaching\n");
  send.set_packet("OK");
}

//------------------------------------------------------------------------------
// Read general registers

void GDBServer::handle_g() {
  recv.take('g');

  if (!recv.error) {
    send.start_packet();
    for (int i = 0; i < 16; i++) {
      send.put_hex_u32(wire->get_gpr(i));
    }
    send.put_hex_u32(wire->get_csr(CSR_DPC));
    send.end_packet();
  }
}

//----------
// Write general registers

void GDBServer::handle_G() {
  recv.take('G');

  for(int i = 0; i < 16; i++) {
    wire->set_gpr(i, recv.take_hex(8));
  }
  wire->set_csr(CSR_DPC, recv.take_hex(8));

  send.set_packet(recv.error ? "E01" : "OK");
}

//------------------------------------------------------------------------------
// Set thread for subsequent operations

void GDBServer::handle_H() {
  recv.take('H');
  recv.skip(1);
  int thread_id = recv.take_hex_signed(); // FIXME do we really need signed here?
  send.set_packet(recv.error ? "E01" : "OK");
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
  int src = recv.take_hex();
  recv.take(',');
  int len = recv.take_hex();

  if (recv.error) {
    printf("\nhandle_m %x %x - recv.error '%s'\n", src, len, recv.buf);
    send.set_packet("");
    return;
  }

  send.start_packet();
  uint32_t buf[256];

  while(len) {
    if ((src & 3) == 0 && len >= 4) {
      int chunk = len & ~3;
      if (chunk > sizeof(buf)) chunk = sizeof(buf);
      wire->get_block_aligned(src, buf, chunk);
      send.put_hex_blob(buf, chunk);
      src += chunk;
      len -= chunk;
    }
    else {
      auto x = wire->get_mem_u8(src);
      send.put_hex_u8(x >> 0);
      src += 1;
      len -= 1;
    }
  }

  send.end_packet();
}

//------------------------------------------------------------------------------
// Write memory

// Does GDB also uses this for flash write? No, I don't think so.

void GDBServer::handle_M() {
  recv.take('M');
  uint32_t dst = recv.take_hex();
  recv.take(',');
  uint32_t len = recv.take_hex();
  recv.take(':');

  if (recv.error) {
    printf("\nhandle_M %x %x - recv.error '%s'\n", dst, len, recv.buf);
    send.set_packet("");
    return;
  }

  uint32_t buf[256];

  while(len) {
    if ((dst & 3) == 0 && len >= 4) {
      int chunk = len & ~3;
      if (chunk > sizeof(buf)) chunk = sizeof(buf);
      recv.take_blob(buf, chunk);
      wire->set_block_aligned(dst, buf, chunk);
      dst += chunk;
      len -= chunk;
    }
    else {
      uint8_t x = (uint8_t)recv.take_hex(2);
      wire->set_mem_u8(dst, x);
      dst += 1;
      len -= 1;
    }
  }

  send.set_packet(recv.error ? "E01" : "OK");
}

//------------------------------------------------------------------------------
// Read the value of register N

void GDBServer::handle_p() {
  recv.take('p');
  int gpr = recv.take_hex();

  if (!recv.error) {
    send.start_packet();
    if (gpr == 16) {
      send.put_hex_u32(wire->get_csr(CSR_DPC));
    }
    else {
      send.put_hex_u32(wire->get_gpr(gpr));
    }
    send.end_packet();
  }
}

//----------
// Write the value of register N

void GDBServer::handle_P() {
  recv.take('P');
  int gpr = recv.take_hex();
  recv.take('=');
  unsigned int val = recv.take_hex();

  if (!recv.error) {
    if (gpr == 16) {
      wire->set_csr(CSR_DPC, val);
    }
    else {
      wire->set_gpr(gpr, val);
    }
    send.set_packet("OK");
  }
}

//------------------------------------------------------------------------------

// > $qRcmd,72657365742030#fc+

void GDBServer::handle_q() {
  //printf("\n");
  //printf("GDBServer::handle_q()\n");
  //printf("%s\n", recv.cursor2);

  if (recv.match_prefix("qAttached")) {
    // Query what GDB is attached to
    // Reply:
    // ‘1’ The remote server attached to an existing process.
    // ‘0’ The remote server created a new process.
    // ‘E NN’ A badly formed request or an error was encountered.
    send.set_packet("1");
  }
  else if (recv.match_prefix("qC")) {
    // -> qC
    // Return current thread ID
    // Reply: ‘QC<thread-id>’
    // can't be -1 or 0, those are special
    send.set_packet("QC1");
  }
  else if (recv.match_prefix("qfThreadInfo")) {
    // Query all active thread IDs
    // can't be -1 or 0, those are special
    send.set_packet("m1");
  }
  else if (recv.match_prefix("qsThreadInfo")) {
    // Query all active thread IDs, continued
    send.set_packet("l");
  }
  else if (recv.match_prefix("qSupported")) {
    // FIXME we're ignoring the contents of qSupported
    recv.cursor2 = recv.buf + recv.size;
    send.set_packet("PacketSize=32768;qXfer:memory-map:read+");
  }
  else if (recv.match_prefix("qXfer:")) {
    if (recv.match_prefix("memory-map:read::")) {
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
  else if (recv.match_prefix("qRcmd,")) {
    // Monitor command

    // reset in hex
    if (recv.match_prefix_hex("reset")) { 
      sl->reset_cpu_and_halt();
      send.set_packet("OK");
    }
  }


  // If we didn't send anything, ignore the command.
  if (send.empty()) {
    recv.cursor2 = recv.buf + recv.size;
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
  sl->step();
  send.set_packet("T05");
}

//------------------------------------------------------------------------------

void GDBServer::handle_v() {
#if 0
  if (recv.match("vCont?")) {
    // FIXME what does this do?
    send.set_packet("");
  }
  else
#endif
  if (recv.match_prefix("vFlash")) {
    if (recv.match_prefix("Write")) {
      recv.take(':');
      int addr = recv.take_hex();
      recv.take(":");
      while((recv.cursor2 - recv.buf) < recv.size) {
        put_flash_cache(addr++, recv.take_char());
      }
      send.set_packet("OK");
    }
    else if (recv.match_prefix("Done")) {
      flush_flash_cache();
      send.set_packet("OK");
    }
    else if (recv.match_prefix("Erase")) {
      recv.take(':');
      int addr = recv.take_hex();
      recv.take(',');
      int size = recv.take_hex();

      if (recv.error) {
        printf("\nBad vFlashErase packet!\n");
        send.set_packet("E00");
        return;
      }
      flash_erase(addr, size);

    }
  }
  else if (recv.match_prefix("vKill")) {
    // FIXME should reset cpu or something?
    recv.cursor2 = recv.buf + recv.size;
    sl->reset_cpu_and_halt();
    send.set_packet("OK");
  }
  else if (recv.match_prefix("vMustReplyEmpty")) {
    send.set_packet("");
  }
  else {
    recv.cursor2 = recv.buf + recv.size;
    send.set_packet("");
  }
}

//------------------------------------------------------------------------------

void GDBServer::handle_z0() {
  recv.take("z0,");
  uint32_t addr = recv.take_hex();
  recv.take(',');
  uint32_t kind = recv.take_hex();

  //printf("GDBServer::handle_z0 0x%08x 0x%08x\n", addr, kind);
  sl->clear_breakpoint(addr, kind);

  send.set_packet("OK");
}

void GDBServer::handle_Z0() {
  recv.take("Z0,");
  uint32_t addr = recv.take_hex();
  recv.take(',');
  uint32_t kind = recv.take_hex();

  //printf("GDBServer::handle_Z0 0x%08x 0x%08x\n", addr, kind);
  sl->set_breakpoint(addr, kind);

  send.set_packet("OK");
}

//------------------------------------------------------------------------------

void GDBServer::handle_z1() {
  recv.take("z1,");
  uint32_t addr = recv.take_hex();
  recv.take(',');
  uint32_t kind = recv.take_hex();

  //printf("GDBServer::handle_z1 0x%08x 0x%08x\n", addr, kind);
  sl->clear_breakpoint(addr, kind);

  send.set_packet("OK");
}

void GDBServer::handle_Z1() {
  recv.take("Z1,");
  uint32_t addr = recv.take_hex();
  recv.take(',');
  uint32_t kind = recv.take_hex();

  //printf("GDBServer::handle_Z1 0x%08x 0x%08x\n", addr, kind);
  sl->set_breakpoint(addr, kind);

  send.set_packet("OK");
}

//------------------------------------------------------------------------------

void GDBServer::flash_erase(int addr, int size) {
  // Erases must be page-aligned
  if ((addr & 0x3F) || (size & 0x3F)) {
    printf("\nBad vFlashErase - addr %x size %x\n", addr, size);
    send.set_packet("E00");
    return;
  }

  while(1) {
    if (addr == 0x00000000 && size == 0x4000) {
      //printf("erase chip 0x%08x\n", addr);
      sl->wipe_chip();
      send.set_packet("OK");
      addr = 0x4000;
      size = 0;
    }
    else if (((addr & 0x3FF) == 0) && (size >= 1024)) {
      //printf("erase sector 0x%08x\n", addr);
      sl->wipe_sector(addr);
      addr += 1024;
      size -= 1024;
    }
    else {
      //printf("erase page 0x%08x\n", addr);
      sl->wipe_page(addr);
      addr += 64;
      size -= 64;
    }
    if (size == 0) break;
  }

  send.set_packet("OK");
}

//------------------------------------------------------------------------------

void GDBServer::put_flash_cache(int addr, uint8_t data) {
  int page_offset = addr & 0x3F;
  int page_base   = addr & ~0x3F;

  if (this->page_bitmap && page_base != this->page_base) {
    flush_flash_cache();
  }
  this->page_base = page_base;

  if (this->page_bitmap & (1 << page_offset)) {
    printf("\nByte in flash page written multiple times\n");
  }
  else {
    this->page_cache[page_offset] = data;
    this->page_bitmap |= (1ull << page_offset);
  }
}

//------------------------------------------------------------------------------

void GDBServer::flush_flash_cache() {
  if (page_base == -1) return;

  if (!page_bitmap) {
    // empty page cache, nothing to do
    //printf("empty page write at    0x%08x\n", this->page_base);
  }
  else  {
    if (page_bitmap == 0xFFFFFFFFFFFFFFFF) {
      // full page write
      //printf("full page write at    0x%08x\n", this->page_base);
    }
    else {
      //printf("partial page write at 0x%08x, mask 0x%016llx\n", this->page_base, this->page_bitmap);
    }

    sl->write_flash2(page_base, page_cache, SLDebugger::page_size);
  }

  this->page_bitmap = 0;
  this->page_base = -1;
  for (int i = 0; i < 64; i++) this->page_cache[i] = 0xFF;
}

//------------------------------------------------------------------------------

void GDBServer::handle_packet() {
  handler_func h = nullptr;
  for (int i = 0; i < handler_count; i++) {
    if (cmp(handler_tab[i].name, recv.buf) == 0) {
      h = handler_tab[i].handler;
    }
  }

  if (h) {
    recv.cursor2 = recv.buf;
    send.clear();
    (*this.*h)();

    if (recv.error) {
      printf("\nParse failure for packet!\n");
      send.set_packet("E00");
    }
    else if ((recv.cursor2 - recv.buf) != recv.size) {
      printf("\nLeftover text in packet - \"%s\"\n", recv.cursor2);
    }
  }
  else {
    printf("\nNo handler for command %s\n", recv.buf);
    send.set_packet("");
  }

  if (!send.packet_valid) {
    printf("\nNot responding to command {%s}\n", recv.buf);
  }
}

//------------------------------------------------------------------------------

void GDBServer::on_hit_breakpoint() {
  printf("Breaking\n");
  send.set_packet("T05");
  state = SEND_PREFIX;
}

//------------------------------------------------------------------------------

void GDBServer::update(bool byte_ie, char byte_in, char& byte_out, bool& byte_oe) {
  auto next_state = state;

  byte_out = 0;
  byte_oe = 0;

  switch(state) {
    case INIT: {
      next_state = IDLE;
      break;
    }

    case IDLE: {
      if (byte_ie) {
        // Wait for start char
        if (byte_in == '$') {
          next_state = RECV_PACKET;
          recv.clear();
          checksum = 0;
        }
        else if (byte_in == '\x003') {
          // We were waiting for a packet, but got a break character.
          // Halt the CPU and report that we halted because of an interrupt.
          printf("Breaking\n");
          sl->halt();
          send.set_packet("T05");
          next_state = SEND_PREFIX;
        }
      }
      break;
    }

    case RECV_PACKET: {
      if (byte_ie) {
        // Add bytes to packet until we see the end char
        // Checksum is for the _escaped_ data.
        if (byte_in == '#') {
          expected_checksum = 0;
          next_state = RECV_SUFFIX1;
        }
        else if (byte_in == '}') {
          checksum += byte_in;
          next_state = RECV_PACKET_ESCAPE;
        }
        else {
          checksum += byte_in;
          recv.put(byte_in);
        }
      }
      break;
    }

    case RECV_PACKET_ESCAPE: {
      if (byte_ie) {
        checksum += byte_in;
        recv.put(byte_in ^ 0x20);
        next_state = RECV_PACKET;
      }
      break;
    }

    case RECV_SUFFIX1: {
      if (byte_ie) {
        expected_checksum = (expected_checksum << 4) | from_hex(byte_in);
        next_state = RECV_SUFFIX2;
      }
      break;
    }

    case RECV_SUFFIX2: {
      if (byte_ie) {
        expected_checksum = (expected_checksum << 4) | from_hex(byte_in);

        if (checksum != expected_checksum) {
          printf("\n");
          printf("Packet transmission error\n");
          printf("expected checksum 0x%02x\n", expected_checksum);
          printf("actual checksum   0x%02x\n", checksum);
          byte_out = '-';
          byte_oe = true;
          next_state = IDLE;
        }
        else {
          // Packet checksum OK, handle it.
          byte_out = '+';
          byte_oe = true;
          printf("\n"); // REMOVE THIS LATER
          handle_packet();
          next_state = send.packet_valid ? SEND_PREFIX : IDLE;
        }
      }
      break;
    }

    case SEND_PREFIX: {
      //printf("\n<< ");
      byte_out = '$';
      byte_oe = true;
      checksum = 0;
      next_state = send.size ? SEND_PACKET : SEND_SUFFIX1;
      send.cursor2 = send.buf;
      break;
    }

    case SEND_PACKET: {
      char c = *send.cursor2;
      if (c == '#' || c == '$' || c == '}' || c == '*') {
        checksum += '}';
        byte_out = '}';
        byte_oe = true;
        next_state = SEND_PACKET_ESCAPE;
        break;
      }
      else {
        checksum += c;
        byte_out = c;
        byte_oe = true;
        send.cursor2++;
        if ((send.cursor2 - send.buf) == send.size) {
          next_state = SEND_SUFFIX1;
        }
        break;
      }
    }

    case SEND_PACKET_ESCAPE: {
      char c = *send.cursor2;
      checksum += c ^ 0x20;
      byte_out = c ^ 0x20;
      byte_oe = true;
      next_state = SEND_PACKET;
      break;
    }

    case SEND_SUFFIX1:
      byte_out = '#';
      byte_oe = true;
      next_state = SEND_SUFFIX2;
      break;

    case SEND_SUFFIX2:
      byte_out = to_hex((checksum >> 4) & 0xF);
      byte_oe = true;
      next_state = SEND_SUFFIX3;
      break;

    case SEND_SUFFIX3:
      byte_out = to_hex((checksum >> 0) & 0xF);
      byte_oe = true;
      next_state = RECV_ACK;
      break;


    case RECV_ACK: {
      if (byte_ie) {
        if (byte_in == '+') {
          //printf("\n>> ");
          next_state = IDLE;
        }
        else if (byte_in == '-') {
          printf("========================\n");
          printf("========  NACK  ========\n");
          printf("========================\n");
          next_state = SEND_PACKET;
        }
        else {
          printf("garbage ack char %d '%c'\n", byte_in, byte_in);
        }
      }
      break;
    }
  }

#if 1
  if (byte_ie) printf(isprint(byte_in)  ? "%c" : "{%02x}", byte_in);
  if (byte_oe) printf(isprint(byte_out) ? "%c" : "{%02x}", byte_out);

  if (state != IDLE && next_state == IDLE) {
    printf("\nCommand count %d", wire->cmd_count);
    wire->cmd_count = 0;
    printf("\n>> ");
  }

  if (state != SEND_PREFIX && next_state == SEND_PREFIX) {
    printf("\n<<   ");
  }
#endif

  state = next_state;

}

//------------------------------------------------------------------------------
