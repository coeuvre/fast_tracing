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

bool buf_starts_with(Buf buf, Buf prefix) {
    if (buf.size < prefix.size) {
        return false;
    }
    return memcmp(buf.data, prefix.data, prefix.size) == 0;
}
