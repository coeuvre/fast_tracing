
#include "src/common.h"

bool BufEql(Buf lhs, Buf rhs) {
  if (lhs.len != rhs.len) {
    return false;
  }

  for (usize i = 0; i < lhs.len; ++i) {
    if (lhs.ptr[i] != rhs.ptr[i]) {
      return false;
    }
  }

  return true;
}