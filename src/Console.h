#pragma once
#include "Packet.h"

struct RVDebug;
struct WCHFlash;
struct Breakpoints;

struct Console {
  Console(RVDebug* rvd, WCHFlash* flash, Breakpoints* soft);
  void reset();
  void dump();
  void start();
  void print_help();

  void update(bool ser_ie, char ser_in);
  void dispatch_command();

  Packet packet;

  RVDebug* rvd;
  WCHFlash* flash;
  Breakpoints* soft;
};
