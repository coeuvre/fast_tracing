#include "src/buf.h"

#include <cstring>

bool buf_equal(Buf lhs, Buf rhs) {
    if (lhs.size != rhs.size) {
        return false;
    }

    return memcmp(lhs.data, rhs.data, lhs.size) == 0;
}

// buf[start, end)
Buf buf_slice(Buf buf, usize start, usize end) {
    ASSERT(start <= end);
    ASSERT(end <= buf.size);
    return Buf{.data = (u8 *)buf.data + start, .size = end - start};
}
