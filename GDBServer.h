#pragma once
#include "utils.h"
#include "SLDebugger.h"
#include "GDBPacket.h"

//------------------------------------------------------------------------------

struct GDBServer {
public:

  GDBServer();
  void init(SLDebugger* sl);

  void update(bool connected, char byte_in, bool byte_ie, char& byte_out, bool& byte_oe);

//private:

  using handler_func = void (GDBServer::*)(void);

  struct handler {
    const char* name;
    handler_func handler;
  };

  static const handler handler_tab[];
  static const int handler_count;

  void handle_questionmark();
  void handle_bang();
  void handle_ctrlc();

  void handle_c();
  void handle_D();
  void handle_g();
  void handle_G();
  void handle_H();
  void handle_k();
  void handle_m();
  void handle_M();
  void handle_p();
  void handle_P();
  void handle_q();
  void handle_Q();
  void handle_R();
  void handle_s();
  void handle_v();
  void handle_z();
  void handle_Z();

//private:

  void handle_packet();

  void flash_erase(int addr, int size);
  void put_flash_cache(int addr, uint8_t data);
  void flush_flash_cache();

  SLDebugger* sl;
  GDBPacket   send;
  GDBPacket   recv;

  int flash_cursor;
  uint64_t flash_write_bitmap;

  uint8_t  page_cache[64];
  int      page_base = -1;
  uint64_t page_bitmap = 0;

  enum {
    RECV_PREFIX,
    RECV_PACKET,
    RECV_PACKET_ESCAPE,
    RECV_SUFFIX1,
    RECV_SUFFIX2,

    SEND_PREFIX,
    SEND_PACKET,
    SEND_PACKET_ESCAPE,
    SEND_SUFFIX1,
    SEND_SUFFIX2,
    SEND_SUFFIX3,

    RECV_ACK,
  };

  int state = RECV_PREFIX;
  char expected_checksum = 0;
  uint8_t checksum = 0;
};

//------------------------------------------------------------------------------
