#pragma once

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

  int take_i32() {
    int sign = match('-') ? -1 : 1;

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
    return sign * accum;
  }

  unsigned int take_u32() {
    unsigned int accum = 0;
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
    //checksum = 0;
  }

  void put_buf(char c) {
    buf[cursor++] = c;
    //checksum += c;
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

  void end_packet() {
    packet_valid = true;
  }

  //----------------------------------------

  int    sentinel1 = 0xDEADBEEF;
  char   buf[32768/4];
  int    sentinel2 = 0xF00DCAFE;
  int    size = 0;
  //char   checksum = 0;
  bool   error = false;
  int    cursor = 0;
  bool   packet_valid = false;
};
