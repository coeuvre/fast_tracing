
#include "src/common.h"

bool BufEql(Buf lhs, Buf rhs) {
  bool result = lhs.len == rhs.len;
  if (result) {
    for (int i = 0; i < lhs.len; ++i) {
      if (lhs.ptr[i] != rhs.ptr[i]) {
        result = false;
        break;
      }
    }
  }
  return result;
}