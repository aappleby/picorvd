#include "Packet.h"
#include <string.h>
#include <stdio.h>

//------------------------------------------------------------------------------

bool parse_binary_literal(const char*& cursor, int& out) {
  int accum = 0;
  int sign = 1;
  int digits = 0;

  while (*cursor && !isspace(*cursor)) {
    if (*cursor >= '0' && *cursor <= '1') {
      accum = accum * 2 + (*cursor - '0');
    }
    else {
      return false;
    }
    digits++;
    cursor++;
  }

  if (digits != 0) {
    out = accum;
    return true;
  }
  else {
    return false;
  }
}

//------------------------------------------------------------------------------

bool parse_decimal_literal(const char*& cursor, int& out) {
  int accum = 0;
  int sign = 1;
  int digits = 0;

  if (*cursor == '-') {
    sign = -1;
    cursor++;
  }

  while (*cursor && !isspace(*cursor)) {
    if (*cursor >= '0' && *cursor <= '9') {
      accum = accum * 10 + (*cursor - '0');
    }
    else {
      return false;
    }
    digits++;
    cursor++;
  }

  if (digits != 0) {
    out = sign * accum;
    return true;
  }
  else {
    return false;
  }
}

//------------------------------------------------------------------------------

bool parse_hex_literal(const char*& cursor, int& out) {
  int accum = 0;
  int sign = 1;
  int digits = 0;

  while (*cursor && !isspace(*cursor)) {
    if (*cursor >= '0' && *cursor <= '9') {
      accum = accum * 16 + (*cursor - '0');
    }
    else if (*cursor >= 'a' && *cursor <= 'f') {
      accum = accum * 16 + (*cursor - 'a' + 10);
    }
    else if (*cursor >= 'A' && *cursor <= 'F') {
      accum = accum * 16 + (*cursor - 'A' + 10);
    }
    else {
      return false;
    }
    digits++;
    cursor++;
  }

  if (digits != 0) {
    out = accum;
    return true;
  }
  else {
    return false;
  }
}

//------------------------------------------------------------------------------

bool parse_octal_literal(const char*& cursor, int& out) {
  int accum = 0;
  int sign = 1;
  int digits = 0;

  if (*cursor == '-') {
    sign = -1;
    cursor++;
  }

  while (*cursor && !isspace(*cursor)) {
    if (*cursor >= '0' && *cursor <= '7') {
      accum = accum * 8 + (*cursor - '0');
    }
    else {
      return false;
    }
    digits++;
    cursor++;
  }

  if (digits != 0) {
    out = sign * accum;
    return true;
  }
  else {
    return false;
  }
}

//------------------------------------------------------------------------------

bool parse_int_literal(const char*& cursor, int& out) {
  auto old_cursor = cursor;
  int digit_count = 0;
  int accum = 0;
  int sign = 1;

  // Skip leading whitespace
  while (isspace(*cursor)) cursor++;

  if (*cursor != '0') {
    auto result = parse_decimal_literal(cursor, out);
    if (!result) cursor = old_cursor;
    return result;
  }
  cursor++;

  if (*cursor == 'b') {
    cursor++;
    auto result = parse_binary_literal(cursor, out);
    if (!result) cursor = old_cursor;
    return result;
  }
  else if (*cursor == 'x') {
    cursor++;
    auto result = parse_hex_literal(cursor, out);
    if (!result) cursor = old_cursor;
    return result;
  }
  else {
    auto result = parse_octal_literal(cursor, out);
    if (!result) cursor = old_cursor;
    return result;
  }
}

//------------------------------------------------------------------------------

#define CHECK2(A) { if (!(A)) { while (1) *(uint32_t*)0xDEADBEEF = 0xF00DCAFE; } };

static struct autotest {
  autotest() {
    int out = 0;
    const char* cursor;

    cursor = "    12345     asdlkjfask ";
    CHECK2(parse_int_literal(cursor, out) && out == 12345);

    cursor = "   -12345     asdlkjfask   ";
    CHECK2(parse_int_literal(cursor, out) && out == -12345);

    cursor = "0x12345";
    CHECK2(parse_int_literal(cursor, out) && out == 0x12345);

    cursor = "0xFEDCBA01";
    CHECK2(parse_int_literal(cursor, out) && out == 0xFEDCBA01);

    cursor = "0b10101100";
    CHECK2(parse_int_literal(cursor, out) && out == 0b10101100);

    cursor = "01234567";
    CHECK2(parse_int_literal(cursor, out) && out == 01234567);

    cursor = "not_a_number";
    CHECK2(!parse_int_literal(cursor, out));

    cursor = "1234badsuffix";
    CHECK2(!parse_int_literal(cursor, out));

    cursor = " 1234 sometest";
    CHECK2(parse_int_literal(cursor, out) && out == 1234 && strcmp(cursor, " sometest") == 0);
  }

} _autotest;

//------------------------------------------------------------------------------
