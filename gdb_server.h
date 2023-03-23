#pragma once
#include "utils.h"
#include "log.h"

//------------------------------------------------------------------------------

struct GDBServer {
public:

  GDBServer(getter cb_get, putter cb_put, Log log)
  : cb_get(cb_get), cb_put(cb_put), log(log) {}

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

  void packet_start();
  void packet_data(const char* buf, int len);
  void packet_string(const char* buf);
  void packet_u8(char x);
  void packet_u32(int x);
  void packet_end();
  bool wait_packet_ack();

  void send_packet(const char* packet, int len);
  void send_packet(const char* packet);
  void send_ack();
  void send_nack();
  void send_ok();
  void send_empty();
  void send_nothing();

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

  void dispatch_command();

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

  Log    log;
  getter cb_get = nullptr;
  putter cb_put = nullptr;

  int     state = WAIT_FOR_START;
  int     serial_fd = 0;
  char    packet[512];
  int     packet_size = 0;
  char*   packet_cursor = 0;
  char    send_checksum = 0;
  char    recv_checksum = 0;

  bool sending = true;
};

//------------------------------------------------------------------------------
