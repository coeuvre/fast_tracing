#ifndef FAST_TRACING_SRC_COMMON_H_
#define FAST_TRACING_SRC_COMMON_H_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define ASSERT(e) assert(e)

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef size_t usize;
typedef ptrdiff_t isize;

typedef float f32;
typedef double f64;

struct Buf {
  u8 *ptr;
  usize len;
};

bool BufEql(Buf lhs, Buf rhs);

#define STR_LITERAL(s) \
  Buf { .ptr = (u8 *)s, .len = sizeof(s) - 1 }

#endif  // FAST_TRACING_SRC_COMMON_H_
