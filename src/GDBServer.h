#pragma once
#include "utils.h"
#include "Packet.h"

struct RVDebug;
struct WCHFlash;
struct SoftBreak;

//------------------------------------------------------------------------------

struct GDBServer {
public:

  GDBServer(RVDebug* rvd, WCHFlash* flash, SoftBreak* soft);
  void reset();
  void dump();

  void update(bool connected, bool byte_ie, char byte_in, bool& byte_oe, char& byte_out);

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

  RVDebug* rvd = nullptr;
  WCHFlash* flash = nullptr;
  SoftBreak* soft = nullptr;

  Packet   send;
  Packet   recv;

  uint8_t* page_cache;
  int      page_base = -1;
  uint64_t page_bitmap = 0;

  enum {
    DISCONNECTED,
    RUNNING,
    KILLED,
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

  int state = DISCONNECTED;
  int next_state = DISCONNECTED;
  char expected_checksum = 0;
  uint8_t checksum = 0;
  uint32_t last_halt_check;
};

//------------------------------------------------------------------------------
