#pragma once
#include "Packet.h"

struct SLDebugger;

struct Console {
  void init(SLDebugger* sl);
  void update(bool ser_ie, char ser_in);
  void dispatch_command();
  SLDebugger* sl = nullptr;
  Packet packet;
};
