#pragma once
#include <stdarg.h>

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
