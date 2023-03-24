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

bool from_hex(char c, int& out) {
  if (c >= '0' && c <= '9') { out = c - '0';      return true; }
  if (c >= 'a' && c <= 'f') { out = 10 + c - 'a'; return true; }
  if (c >= 'A' && c <= 'F') { out = 10 + c - 'A'; return true; }
  return false;
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

bool atoi2(const char* cursor, int& out) {
  int sign = 1;
  if (*cursor == '-') {
    sign = -1;
    cursor++;
  }

  bool any_digits = false;
  int accum = 0;
  while(*cursor) {
    if (*cursor < '0' || *cursor > '9') break;
    any_digits = true;
    accum *= 10;
    accum += *cursor++ - '0';
  }

  if (any_digits) {
    out = sign * accum;
    return true;
  }
  else {
    return false;
  }
}

char* atox(char* cursor, int& out) {
  if (!cursor) return cursor;

  int sign = 1;
  if (*cursor == '-') {
    sign = -1;
    cursor++;
  }

  bool any_digits = false;
  int accum = 0;
  int digit = 0;
  while(from_hex(*cursor, digit)) {
    any_digits = true;
    accum = (accum << 4) | digit;
    cursor++;
  }
  out = sign * accum;
  return any_digits ? cursor : nullptr;
}
