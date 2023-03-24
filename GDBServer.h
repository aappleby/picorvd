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
  void handle_qAttached();
  void handle_qC();
  void handle_qfThreadInfo();
  void handle_qL();
  void handle_qOffsets();
  void handle_qsThreadInfo();
  void handle_qSupported();
  void handle_qSymbol();
  void handle_qTfP();
  void handle_qTfV();
  void handle_qTStatus();
  void handle_s();
  void handle_vCont();
  void handle_vKill();
  void handle_vMustReplyEmpty();

private:

  enum {
    WAIT_FOR_START,
    RECV_COMMAND,
    RECV_ARGS,
    RECV_CHECKSUM1,
    RECV_CHECKSUM2,
    SEND_ACK,
    SEND_PACKET,
    WAIT_ACK,
  };

  SLDebugger* sl;
  Log    log;
  getter cb_get = nullptr;
  putter cb_put = nullptr;
  bool   sending = true;

  int    state = WAIT_FOR_START;
  int    serial_fd = 0;

  GDBPacket send;
  GDBPacket recv;
};

//------------------------------------------------------------------------------
