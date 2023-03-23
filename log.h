#pragma once
#include "utils.h"
#include <stdarg.h>
#include <stdio.h>

//------------------------------------------------------------------------------

class Log {
public:
  Log(putter cb_put) : cb_put(cb_put) {}

  void operator()(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint_to(cb_put, fmt, args);
  }

  void R(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint_color(0xAAAAFF, fmt, args);
  }

  void G(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint_color(0xAAFFAA, fmt, args);
  }

  void B(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprint_color(0xFFAAAA, fmt, args);
  }

private:

  void vprint_color(unsigned int color, const char* fmt, va_list args) {
    set_color(color);
    vprint_to(cb_put, fmt, args);
    clear_color();
  }

  void set_color(unsigned int color) {
    print_to(cb_put, "\u001b[38;2;%d;%d;%dm", (color >> 0) & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
  }

  void clear_color() {
    print_to(cb_put, "\u001b[0m");
  }

  putter cb_put;
};

//------------------------------------------------------------------------------
