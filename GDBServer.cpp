#include "GDBServer.h"
#include "utils.h"
#include "WCH_Regs.h"
#include <ctype.h>

const char* memory_map = R"(<?xml version="1.0"?>
<!DOCTYPE memory-map PUBLIC "+//IDN gnu.org//DTD GDB Memory Map V1.0//EN" "http://sourceware.org/gdb/gdb-memory-map.dtd">
<memory-map>
  <memory type="flash" start="0x08000000" length="0x4000">
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
    sl->set_mem(addr, recv.take_i32());
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
    sl->set_gpr(gpr, val);
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
      while(recv.cursor < recv.size) {
        put_flash_cache(addr++, recv.take_char());
      }
      send.set_packet("OK");
    }
    else if (recv.match("Done")) {
      flush_flash_cache();
      send.set_packet("OK");
    }
    else if (recv.match("Erase")) {
      recv.take(':');
      int addr = recv.take_i32();
      recv.take(',');
      int size = recv.take_i32();

      if (recv.error) {
        log("\nBad vFlashErase packet!\n");
        send.set_packet("E00");
        return;
      }
      flash_erase(addr, size);

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

void GDBServer::flash_erase(int addr, int size) {
  // Erases must be page-aligned
  if ((addr & 0x3F) || (size & 0x3F)) {
    log("\n");
    log("Bad vFlashErase - addr %x size %x\n", addr, size);
    send.set_packet("E00");
    return;
  }

  while(1) {
    if (addr == 0x08000000 && size == 0x4000) {
      log("erase chip 0x%08x\n", addr);
      sl->wipe_chip();
      send.set_packet("OK");
      addr = 0x4000;
      size = 0;
    }
    else if (((addr & 0x3FF) == 0) && (size >= 1024)) {
      log("erase sector 0x%08x\n", addr);
      sl->wipe_sector(addr);
      addr += 1024;
      size -= 1024;
    }
    else {
      log("erase page 0x%08x\n", addr);
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

  if (page_base != this->page_base) {
    flush_flash_cache();
    this->page_base = page_base;
  }

  if (this->page_bitmap & (1 << page_offset)) {
    log("\nByte in flash page written multiple times\n");
  }
  else {
    this->page_cache[page_offset] = data;
    this->page_bitmap |= (1ull << page_offset);
  }
}

//------------------------------------------------------------------------------

void GDBServer::flush_flash_cache() {
  if (page_bitmap == 0) {
    // empty page cache, nothing to do
    //log("empty page write at    0x%08x\n", this->page_base);
  }
  else if (page_bitmap == 0xFFFFFFFFFFFFFFFF) {
    // full page write
    log("full page write at    0x%08x\n", this->page_base);

    uint32_t* cursor = (uint32_t*)page_cache;
    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < 4; x++) {
        log("0x%08x ", cursor[x + y * 4]);
      }
      log("\n");
    }
    sl->write_flash(page_base, (uint32_t*)page_cache, 16);
  }
  else {
    // partial page write
    log("partial page write at 0x%08x, mask 0x%016llx\n", this->page_base, this->page_bitmap);

    // Merge the page cache with the current page in flash
    uint32_t current_page[16];
    for (int i = 0; i < 16; i++) {
      current_page[i] = sl->get_mem(page_base + 4*i);
    }



    uint8_t* cursor = (uint8_t*)current_page;
    for (int i = 0; i < 64; i++) {
      if ((page_bitmap & (1ull << i)) == 0) {
        page_cache[i] = cursor[i];
      }
    }

    uint32_t* cursor2 = (uint32_t*)page_cache;
    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < 4; x++) {
        log("0x%08x ", cursor2[x + y * 4]);
      }
      log("\n");
    }

    sl->write_flash(page_base, (uint32_t*)page_cache, 16);
  }

  this->page_bitmap = 0;
  this->page_base = -1;
  for (int i = 0; i < 64; i++) this->page_cache[i] = 0;
}

//------------------------------------------------------------------------------

void GDBServer::handle_packet() {
  if (checksum != expected_checksum) {
    log("\n");
    log("Packet transmission error\n");
    log("expected checksum 0x%02x\n", expected_checksum);
    log("actual checksum   0x%02x\n", checksum);
    put_byte('-');
    return;
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
}

//------------------------------------------------------------------------------

void GDBServer::update(bool connected, char c) {
  switch(state) {
    case S_DISCONNECTED:
      break;

    case S_PREFIX:
      // Wait for start char
      if (c == '$') {
        state = S_PACKET;
        recv.clear();
        checksum = 0;
      }
      break;

    case S_PACKET:
      // Add bytes to packet until we see the end char
      // Checksum is for the _escaped_ data.
      if (c == '#') {
        expected_checksum = 0;
        state = S_SUFFIX1;
      }
      else if (c == '}') {
        checksum += c;
        state = S_PACKET_ESCAPE;
      }
      else {
        checksum += c;
        recv.put_buf(c);
      }
      break;

    case S_PACKET_ESCAPE:
      checksum += c;
      recv.put_buf(c ^ 0x20);
      break;

    case S_SUFFIX1:
      expected_checksum = (expected_checksum << 4) | from_hex(c);
      state = S_SUFFIX2;
      break;

    case S_SUFFIX2:
      expected_checksum = (expected_checksum << 4) | from_hex(c);
      handle_packet();
      sending = true;
      state = S_REPLY;
      break;

    case S_REPLY:
      for (int retry = 0; retry < 5; retry++) {
        if (send_packet()) break;
        if (retry == 4) {
          log("\nFailed to send packet after 5 retries\n");
        }
      }
      sending = false;
      state = S_PREFIX;
      break;
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
