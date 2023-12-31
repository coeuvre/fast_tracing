#pragma once

#include "src/defs.h"

struct Buf {
  u8 *data;
  usize size;
};

bool buf_equal(Buf lhs, Buf rhs);
Buf buf_slice(Buf buf, usize start, usize end);
bool buf_starts_with(Buf buf, Buf prefix);

#define STR_LITERAL(s) \
  Buf { .data = (u8 *)s, .size = sizeof(s) - 1 }
