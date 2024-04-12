#pragma once
#include <stdint.h>
#include "utils.h"
#include <ctype.h>
#include "string.h"

bool parse_int_literal(const char*& cursor, int& out);

enum class ParseError {
  ERROR
};

struct Packet {

  void clear() {
    memset(buf, 0, sizeof(buf));
    size = 0;
    error = false;
    cursor2 = buf;
    packet_valid = false;
  }

  //----------------------------------------

  char to_hex(int x) {
    if (x >= 0  && x <= 9)  return '0' + x;
    if (x >= 10 && x <= 15) return 'A' - 10 + x;
    return '?';
  }

  bool from_hex(char c, int& x) {
    if (c >= '0' && c <= '9') {
      x = (c - '0');
      return true;
    }
    else if (c >= 'a' && c <= 'f') {
      x = (10 + c - 'a');
      return true;
    }
    else if (c >= 'A' && c <= 'F') {
      x = (10 + c - 'A');
      return true;
    }
    else {
      return false;
    }
  }

  int from_hex(char c) {
    if (c >= '0' && c <= '9') {
      return (c - '0');
    }
    else if (c >= 'a' && c <= 'f') {
      return (10 + c - 'a');
    }
    else if (c >= 'A' && c <= 'F') {
      return (10 + c - 'A');
    }
    else {
      return -1;
    }
  }

  //----------------------------------------

  bool skip(int d) {
    if ((cursor2 - buf) + d > size) {
      error = true;
      return false;
    }
    cursor2 += d;
    return true;
  }

  //----------------------------------------

  char peek_char() {
    return (cursor2 - buf) >= size ? 0 : *cursor2;
  }

  char take_char() {
    if ((cursor2 - buf) >= size) {
      error = true;
      return 0;
    }
    else {
      return *cursor2++;
    }
  }

  void take(char c) {
    char d = take_char();
    if (c != d) {
      error = true;
    }
  }

  void take(const char* s) {
    while (*s) take(*s++);
  }

  //----------------------------------------
  // Take standard C++-format integer from packet

  Result<int, ParseError> take_int() {
    int out = 0;
    const char* temp = cursor2;
    auto ok = parse_int_literal(temp, out);
    if (ok) {
      cursor2 = (char*)temp;
      return out;
    } else {
      return ParseError::ERROR;
    }
  }

  //----------------------------------------
  // Take fixed-length hex int from packet

  uint32_t take_hex(int digits) {
    uint32_t accum = 0;

    while (isspace(peek_char())) cursor2++;

    for (int i = 0; i < digits; i++) {
      int digit = 0;
      if (!from_hex(peek_char(), digit)) {
        error = true;
        break;
      }
      accum = (accum << 4) | digit;
      cursor2++;
    }

    return error ? 0 : accum;
  }

  //----------------------------------------
  // Take variable-length hex int from packet

  unsigned int take_hex() {
    int accum = 0;
    bool any_digits = false;

    while (isspace(peek_char())) cursor2++;

    int digit = 0;
    for (int i = 0; i < 8; i++) {
      if (!from_hex(peek_char(), digit)) break;
      accum = (accum << 4) | digit;
      any_digits = true;
      cursor2++;
    }

    if (!any_digits) error = true;
    return accum;
  }

  //----------------------------------------

  bool maybe_take_hex(uint32_t& out) {
    auto old_error = error;
    auto old_cursor = cursor2;
    auto new_out = take_hex();
    if (error) {
      error = old_error;
      cursor2 = old_cursor;
      return false;
    }
    else {
      out = new_out;
      return true;
    }
  }

  //----------------------------------------

  typedef Result<uint32_t, ParseError> Res;

  Res take_hex2() {
    auto old_error = error;
    auto old_cursor = cursor2;
    uint32_t out = take_hex();
    if (error) {
      error = old_error;
      cursor2 = old_cursor;
      return ParseError::ERROR;
    }
    else {
      return out;
    }
  }

  //----------------------------------------

  bool maybe_take_hex(int digits, uint32_t& out) {
    auto old_error = error;
    auto old_cursor = cursor2;
    auto new_out = take_hex(digits);
    if (error) {
      error = old_error;
      cursor2 = old_cursor;
      return false;
    }
    else {
      out = new_out;
      return true;
    }
  }

  //----------------------------------------
  // FIXME - do we _ever_ see negative numbers in the packets?

  int take_hex_signed() {
    int sign = match('-') ? -1 : 1;
    return take_hex() * sign;
  }

  //----------------------------------------

  bool take_blob(void* blob, int size) {
    auto old_cursor = cursor2;
    uint8_t* dst = (uint8_t*)blob;

    for (int i = 0; i < size; i++) {
      int lo = 0, hi = 0;
      if (((cursor2 - buf) <= size - 2) &&
          from_hex(cursor2[0], hi) &&
          from_hex(cursor2[1], lo)) {
        *dst++ = (hi << 4) | lo;
        cursor2 += 2;
      }
      else {
        error = true;
        break;
      }
    }

    if (error) cursor2 = old_cursor;
    return !error;
  }

  //----------------------------------------

  bool match(char c) {
    auto match = c && peek_char() == c;
    if (match) cursor2++;
    return match;
  }

  //----------------------------------------

  bool match_word(const char* p) {
    auto c = cursor2;
    for(;*p && *c; p++, c++) {
      if (*p != *c) return false;
    }

    bool match = *p == 0 && (isspace(*c) || *c == 0);
    if (match) cursor2 = c;
    return match;
  }

  bool match_prefix(const char* p) {
    auto c = cursor2;
    for(;*p && *c; p++, c++) {
      if (*p != *c) return false;
    }

    bool match = *p == 0;
    if (match) cursor2 = c;
    return match;
  }

  bool match_prefix_hex(const char* p) {
    auto c = cursor2;

    while (*p) {
      auto hi = (*p >> 4) & 0xF;
      auto lo = (*p >> 0) & 0xF;

      if (from_hex(*c++) != hi) return false;
      if (from_hex(*c++) != lo) return false;
      p++;
    }

    cursor2 = c;
    return true;
  }

  //----------------------------------------

  void set_packet(const char* text) {
    start_packet();
    put_str(text);
    end_packet();
  }

  void start_packet() {
    clear();
  }

  void put(char c) {
    *cursor2++ = c;
    size++;
  }

  void put_str(const char* s) {
    while (*s) put(*s++);
  }

  void put_hex_u8(uint8_t x) {
    // must be high nibble first
    put(to_hex((x >> 4) & 0xF));
    put(to_hex((x >> 0) & 0xF));
  }

  void put_hex_u16(uint16_t x) {
    // must be little-endian
    put_hex_u8(x >> 0);
    put_hex_u8(x >> 8);
  }

  void put_hex_u32(uint32_t x) {
    // must be little-endian
    put_hex_u8(x >> 0);
    put_hex_u8(x >> 8);
    put_hex_u8(x >> 16);
    put_hex_u8(x >> 24);
  }

  void put_hex_blob(void* blob, int size) {
    uint8_t* src = (uint8_t*)blob;
    for (int i = 0; i < size; i++) {
      put_hex_u8(src[i]);
    }
  }

  void end_packet() {
    packet_valid = true;
  }

  bool empty() {
    return cursor2 == buf;
  }

  //----------------------------------------

  int    sentinel1 = 0xDEADBEEF;
  char   buf[16384];
  int    sentinel2 = 0xF00DCAFE;
  int    size = 0;
  bool   error = false;
  //int    cursor = 0;
  char*  cursor2 = buf;
  bool   packet_valid = false;
};
