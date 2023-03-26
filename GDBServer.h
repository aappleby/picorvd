#pragma once
#include "utils.h"
#include "log.h"
#include "SLDebugger.h"
#include "GDBPacket.h"

//------------------------------------------------------------------------------

struct GDBServer {
public:

  GDBServer(SLDebugger* sl, getter cb_get, putter cb_put, Log log)
  : sl(sl), cb_get(cb_get), cb_put(cb_put), log(log) {}

  void loop();

private:

  using handler_func = void (GDBServer::*)(void);

  struct handler {
    const char* name;
    handler_func handler;
  };

  static const handler handler_tab[];
  static const int handler_count;

  void put_byte(char b);
  char get_byte();

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

private:

  bool send_packet();

  SLDebugger* sl;
  GDBPacket   send;
  GDBPacket   recv;
  Log         log;

  getter cb_get = nullptr;
  putter cb_put = nullptr;
  bool   sending = true;
};

//------------------------------------------------------------------------------
