#include "utils.h"
#include <stdarg.h>
#include <stdio.h>

//------------------------------------------------------------------------------

void vprint_to(putter p, const char* fmt, va_list args) {
  char buffer[256];
  int len = vsnprintf(buffer, 256, fmt, args);
  for (int i = 0; i < len; i++) p(buffer[i]);
}

void print_to(putter p, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprint_to(p, fmt, args);
}

//------------------------------------------------------------------------------

char to_hex(int x) {
  if (x >= 0  && x <= 9)  return '0' + x;
  if (x >= 10 && x <= 15) return 'A' - 10 + x;
  return '?';
}

int from_hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + c - 'a';
  if (c >= 'A' && c <= 'F') return 10 + c - 'A';
  return -1;
}

//------------------------------------------------------------------------------

int cmp(const char* prefix, const char* text) {
  while(1) {
    // Note we return when we run out of matching prefix, not when both
    // strings match.
    if (*prefix == 0)    return 0;
    if (*prefix > *text) return -1;
    if (*prefix < *text) return 1;
    prefix++;
    text++;
  }
}

//------------------------------------------------------------------------------

int _atoi(const char* cursor) {
  int sign = 1;
  if (*cursor == '-') {
    sign = -1;
    cursor++;
  }

  int accum = 0;
  while(*cursor) {
    if (*cursor < '0' || *cursor > '9') break;
    accum *= 10;
    accum += *cursor++ - '0';
  }
  return sign * accum;
}
