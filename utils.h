#pragma once
#include <stdarg.h>
#include <stdio.h>

using putter = void (*)(char);
using getter = char (*)();

char to_hex(int x);
int from_hex(char c);
int cmp(const char* prefix, const char* text);
int _atoi(const char* cursor);
bool atoi2(const char* cursor, int& out);
void vprint_to(putter p, const char* fmt, va_list args);
void print_to(putter p, const char* fmt, ...);
char* atox(char* cursor, int& out);

#define CHECK(A) if(!(A)) { printf("ASSERT FAIL %s %d\n", __FILE__, __LINE__); while(1); }
#define CHECK_EQ(A, B) { \
  auto _a = (A); \
  auto _b = (B); \
  if (_a != _b) { \
    printf("ASSERT FAIL: " #A " != " #B " (0x%08x != 0x%08x) @ %s:%d\n", _a, _b, __FILE__, __LINE__); \
    while(1); \
  } \
}


template<typename R, typename E>
struct Result {

  Result(E e) {
    err = true;
    r = R();
    e = e;
  }

  Result(R r) {
    err = false;
    r = r;
    e = E();
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
