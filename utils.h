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

//#define CHECK(A, args...) if(!(A)) { printf("ASSERT FAIL %s %d\n", __FILE__, __LINE__); printf("" args); printf("\n"); while(1); }

#define CHECK(A, args...)

#define CHECK_EQ(A, B) { \
  auto _a = (A); \
  auto _b = (B); \
  if (_a != _b) { \
    printf("ASSERT FAIL: " #A " != " #B " (0x%08x != 0x%08x) @ %s:%d\n", _a, _b, __FILE__, __LINE__); \
    while(1); \
  } \
}

template<typename T>
void set_bit(T* base, int i, bool b) {
  int block = i / (sizeof(T) * 8);
  int index = i % (sizeof(T) * 8);
  base[block] &= ~(1 << index);
  base[block] |=  (b << index);
}

template<typename T>
bool get_bit(T* base, int i) {
  int block = i / (sizeof(T) * 8);
  int index = i % (sizeof(T) * 8);
  return (base[block] >> index) & 1;
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
