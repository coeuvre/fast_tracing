#include "tools/common.h"

void split_arg(char *arg, Buf *out_key, Buf *out_value) {
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
            .data = (u8 *)arg,
            .size = (usize)(p - arg),
        };
    }

    if (out_value) {
        if (p + 1 < end) {
            *out_value = {
                .data = (u8 *)(p + 1),
                .size = (usize)(end - p - 1),
            };
        } else {
            *out_value = {
                .data = 0,
                .size = 0,
            };
        }
    }
}
