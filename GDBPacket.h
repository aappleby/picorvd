#pragma once

struct GDBPacket {

  void clear() {
    for (int i = 0; i < 1024; i++) buf[i] = 0;
    size = 0;
    checksum = 0;
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

  bool skip_char(char c) {
    if (buf[cursor] == c) {
      cursor++;
      return true;
    }
    else {
      error = true;
      return false;
    }
  }

  bool skip_prefix(const char* p) {
    while(*p) {
      if (buf[cursor] == *p) {
        cursor++;
        p++;
      }
      else {
        return false;
      }
    }
    return true;
  }

  char get_char() {
    if (cursor >= size) {
      error = true;
      return 0;
    }
    return buf[cursor++];
  }

  int get_hex() {
    if (cursor >= size) {
      error = true;
      return 0;
    }

    int sign = 1;
    if (buf[cursor] == '-') {
      sign = -1;
      cursor++;
    }

    int accum = 0;
    bool any_digits = false;

    while(1) {
      char c = buf[cursor];
      int digit = 0;
      if (!from_hex(c, digit)) break;
      accum = (accum << 4) | digit;
      any_digits = true;
      cursor++;
    }

    if (!any_digits) error = true;
    return sign * accum;
  }

  //----------------------------------------

  void set_packet(const char* text) {
    start_packet();
    put_str(text);
    end_packet();
  }

  void start_packet() {
    clear();
    put_buf('$');
    checksum = 0;
  }

  void put_buf(char c) {
    buf[cursor++] = c;
    checksum += c;
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
    char old_checksum = checksum;
    put_buf('#');
    put_u8(old_checksum);
    packet_valid = true;
  }

  //----------------------------------------

  int    sentinel1 = 0xDEADBEEF;
  char   buf[1024];
  int    sentinel2 = 0xF00DCAFE;
  int    size = 0;
  char   checksum = 0;
  bool   error = false;
  int    cursor = 0;
  bool   packet_valid = false;
};
