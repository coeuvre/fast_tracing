#include "tools/common.h"

void SplitArg(char *arg, Buf *out_key, Buf *out_value) {
  ASSERT(arg);

  char *p = arg;
  while (*p) {
    if (*p == '=') {
      break;
    }
    p++;
  }

  char *end = p;
  while (*end) {
    end++;
  }

  if (out_key) {
    *out_key = {
        .ptr = (u8 *)arg,
        .len = (usize)(p - arg),
    };
  }

  if (out_value) {
    if (p + 1 < end) {
      *out_value = {
          .ptr = (u8 *)(p + 1),
          .len = (usize)(end - p - 1),
      };
    } else {
      *out_value = {
          .ptr = 0,
          .len = 0,
      };
    }
  }
}