#pragma once
#include <stdint.h>
#include "utils.h"

enum class ParseError {
  ERROR
};

struct GDBPacket {

  void clear() {
    for (int i = 0; i < 1024; i++) buf[i] = 0;
    size = 0;
    //checksum = 0;
    error = false;
    cursor = 0;
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

  //----------------------------------------

  bool skip(int d) {
    if (cursor + d > size) {
      error = true;
      return false;
    }
    cursor += d;
    return true;
  }

  //----------------------------------------

  char peek_char() {
    return cursor >= size ? 0 : buf[cursor];
  }

  char take_char() {
    if (cursor >= size) {
      error = true;
      return 0;
    }
    else {
      return buf[cursor++];
    }
  }

  void take(char c) {
    char d = take_char();
    if (c != d) {
      error = true;
    }
  }

  void take(const char* s) {
    while(*s) take(*s++);
  }

  //----------------------------------------
  // Take variable-length hex int from packet

  unsigned int take_hex() {
    int accum = 0;
    bool any_digits = false;

    int digit = 0;
    for (int i = 0; i < 8; i++) {
      if (!from_hex(peek_char(), digit)) break;
      accum = (accum << 4) | digit;
      any_digits = true;
      cursor++;
    }

    if (!any_digits) error = true;
    return accum;
  }

  //----------------------------------------

  bool maybe_take_hex(uint32_t& out) {
    auto old_error = error;
    auto old_cursor = cursor;
    auto new_out = take_hex();
    if (error) {
      error = old_error;
      cursor = old_cursor;
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
    auto old_cursor = cursor;
    uint32_t out = take_hex();
    if (error) {
      error = old_error;
      cursor = old_cursor;
      return ParseError::ERROR;
    }
    else {
      return out;
    }
  }

  //----------------------------------------
  // Take fixed-length hex int from packet

  uint32_t take_hex(int digits) {
    uint32_t accum = 0;

    for (int i = 0; i < digits; i++) {
      int digit = 0;
      if (!from_hex(peek_char(), digit)) {
        error = true;
        break;
      }
      accum = (accum << 4) | digit;
      cursor++;
    }

    return error ? 0 : accum;
  }

  //----------------------------------------

  bool maybe_take_hex(int digits, uint32_t& out) {
    auto old_error = error;
    auto old_cursor = cursor;
    auto new_out = take_hex(digits);
    if (error) {
      error = old_error;
      cursor = old_cursor;
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
    auto old_cursor = cursor;
    uint8_t* dst = (uint8_t*)blob;

    for (int i = 0; i < size; i++) {
      int lo = 0, hi = 0;
      if ((cursor <= size - 2) &&
          from_hex(buf[cursor + 0], hi) &&
          from_hex(buf[cursor + 1], lo)) {
        *dst++ = (hi << 4) | lo;
        cursor += 2;
      }
      else {
        error = true;
        break;
      }
    }

    if (error) cursor = old_cursor;
    return !error;
  }

  //----------------------------------------

  bool match(char c) {
    int old_cursor = cursor;
    if (take_char() != c) {
      cursor = old_cursor;
      return false;
    }
    return true;
  }

  bool match(const char* p) {
    int old_cursor = cursor;
    while(*p) {
      if (!match(*p++)) {
        cursor = old_cursor;
        return false;
      }
    }
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

  void put_buf(char c) {
    buf[cursor++] = c;
    size++;
  }

  void put_str(const char* s) {
    while(*s) put_buf(*s++);
  }

  void put_u32(unsigned int x) {
    put_u8(x >> 0);
    put_u8(x >> 8);
    put_u8(x >> 16);
    put_u8(x >> 24);
  }

  void put_u8(unsigned char x) {
    put_buf(to_hex((x >> 4) & 0xF));
    put_buf(to_hex((x >> 0) & 0xF));
  }

  void put_blob(void* blob, int size) {
    uint8_t* src = (uint8_t*)blob;
    for (int i = 0; i < size; i++) {
      put_u8(src[i]);
    }
  }

  void end_packet() {
    packet_valid = true;
  }

  //----------------------------------------

  int    sentinel1 = 0xDEADBEEF;
  char   buf[32768/4];
  int    sentinel2 = 0xF00DCAFE;
  int    size = 0;
  bool   error = false;
  int    cursor = 0;
  bool   packet_valid = false;
};
