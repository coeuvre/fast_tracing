#pragma once

#include "src/defs.h"

struct Buf {
  void *data;
  usize size;
};

bool buf_equal(Buf lhs, Buf rhs);
Buf buf_slice(Buf buf, usize start, usize end);

#define STR_LITERAL(s) \
  Buf { .data = (void *)s, .size = sizeof(s) - 1 }
