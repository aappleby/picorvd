#pragma once
#include "utils.h"
#include "SLDebugger.h"
#include "Packet.h"

//------------------------------------------------------------------------------

struct GDBServer {
public:

  GDBServer();
  void init(SLDebugger* sl);

  void update(bool byte_ie, char byte_in, char& byte_out, bool& byte_oe);

//private:

  using handler_func = void (GDBServer::*)(void);

  struct handler {
    const char* name;
    handler_func handler;
  };

  static const handler handler_tab[];
  static const int handler_count;

  void on_connect()    { connected = true; }
  void on_disconnect() { connected = false; }

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
  void handle_z0();
  void handle_Z0();
  void handle_z1();
  void handle_Z1();

//private:

  void handle_packet();
  void on_hit_breakpoint();

  void flash_erase(int addr, int size);
  void put_flash_cache(int addr, uint8_t data);
  void flush_flash_cache();

  bool connected = false;

  SLDebugger* sl = nullptr;
  OneWire*    wire = nullptr;

  Packet   send;
  Packet   recv;

  uint8_t  page_cache[64];
  int      page_base = -1;
  uint64_t page_bitmap = 0;

  enum {
    INIT,
    IDLE,
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

  int state = INIT;
  char expected_checksum = 0;
  uint8_t checksum = 0;
};

//------------------------------------------------------------------------------
