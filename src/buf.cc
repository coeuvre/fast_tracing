#include "src/buf.h"

bool buf_equal(Buf lhs, Buf rhs) {
  if (lhs.size != rhs.size) {
    return false;
  }

  u8 *a = (u8 *)lhs.data;
  u8 *b = (u8 *)rhs.data;
  for (usize i = 0; i < lhs.size; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }

  return true;
}

Buf buf_slice(Buf buf, usize start, usize end) {
  ASSERT(start <= end);
  ASSERT(end <= buf.size);
  return Buf{.data = (u8 *)buf.data + start, .size = end - start};
}