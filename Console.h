#pragma once
#include "Packet.h"

struct RVDebug;
struct WCHFlash;
struct SoftBreak;

struct Console {
  Console(RVDebug* rvd, WCHFlash* flash, SoftBreak* soft);
  void reset();

  void update(bool ser_ie, char ser_in);
  void dispatch_command();

  Packet packet;

  RVDebug* rvd;
  WCHFlash* flash;
  SoftBreak* soft;
};
