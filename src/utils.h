#pragma once
#include <stdarg.h>
#include <stdio.h>

using putter = void (*)(char);
using getter = char (*)();

//------------------------------------------------------------------------------

void printf_color(const char* color, const char* fmt, ...);

#define LOG_R(...) printf_color("1;31", __VA_ARGS__)
#define LOG_G(...) printf_color("1;32", __VA_ARGS__)
#define LOG_Y(...) printf_color("1;33", __VA_ARGS__)
#define LOG_B(...) printf_color("1;34", __VA_ARGS__)
#define LOG_M(...) printf_color("1;35", __VA_ARGS__)
#define LOG_C(...) printf_color("1;36", __VA_ARGS__)
#define LOG_W(...) printf_color("1;37", __VA_ARGS__)
#define LOG(...)   printf(__VA_ARGS__)

#define printf_r(...) printf_color("1;31", __VA_ARGS__)
#define printf_g(...) printf_color("1;32", __VA_ARGS__)
#define printf_y(...) printf_color("1;33", __VA_ARGS__)
#define printf_b(...) printf_color("1;34", __VA_ARGS__)
#define printf_m(...) printf_color("1;35", __VA_ARGS__)
#define printf_c(...) printf_color("1;36", __VA_ARGS__)
#define printf_w(...) printf_color("1;37", __VA_ARGS__)

//------------------------------------------------------------------------------

char to_hex(int x);
int from_hex(char c);
int cmp(const char* prefix, const char* text);
int _atoi(const char* cursor);
bool atoi2(const char* cursor, int& out);
void vprint_to(putter p, const char* fmt, va_list args);
void print_to(putter p, const char* fmt, ...);
char* atox(char* cursor, int& out);

//#define CHECK(A, args...) if(!(A)) { printf_r("ASSERT FAIL %s %d\n", __FILE__, __LINE__); printf_r("" args); printf_r("\n"); while (1); }

#define CHECK(A, args...)

/*
#define CHECK_EQ(A, B) { \
  auto _a = (A); \
  auto _b = (B); \
  if (_a != _b) { \
    printf_r("ASSERT FAIL: " #A " != " #B " (0x%08x != 0x%08x) @ %s:%d\n", _a, _b, __FILE__, __LINE__); \
    while (1); \
  } \
}
*/

inline void set_bit(void* blob, int i, bool b) {
  uint8_t* base = (uint8_t*)blob;
  base[i >> 3] &= ~(1 << (i & 7));
  base[i >> 3] |=  (b << (i & 7));
}

inline bool get_bit(void* blob, int i) {
  uint8_t* base = (uint8_t*)blob;
  return (base[i >> 3] >> (i & 7)) & 1;
}

template<typename T>
bool bit(T& t, int i) {
  return get_bit(&t, i);
}

template<typename R, typename E>
struct Result {

  Result(E e) {
    //printf("result error\n");
    this->err = true;
    this->r = R();
    this->e = e;
  }

  Result(R r) {
    //printf("result ok %d\n", r);
    this->err = false;
    this->r = r;
    this->e = E();
  }

  R ok_or(R default_val) {
    return err ? default_val : r;
  }

  bool is_ok()  const { return !err; }
  bool is_err() const { return err; }

  operator R() const { CHECK(is_ok());  return r; }
  operator E() const { CHECK(is_err()); return e; }

  static Result Ok(R r) {
    Result result;
    result.err = false;
    result.r = r;
    result.e = E();
    return result;
  }

  static Result Err(E e) {
    Result result;
    result.err = true;
    result.r = R();
    result.e = e;
    return result;
  }

  bool err;
  R r;
  E e;
};
