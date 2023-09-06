#pragma once

#include <inttypes.h>
#include <stddef.h>

#ifdef NDEBUG
#undef NDEBUG // Always want assert
#include <assert.h>
#define NDEBUG
#define DEBUG_ASSERT(e)
#else  // NDEBUG
#include <assert.h>
#define DEBUG_ASSERT(e) ASSERT(e)
#endif  // NDEBUG

#define ASSERT(e) assert(e)
#define UNREACHABLE ASSERT(0 && "unreachable")

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

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

template <typename T>
inline T min(T lhs, T rhs) {
  return lhs < rhs ? lhs : rhs;
}

template <typename T>
inline T max(T lhs, T rhs) {
  return lhs > rhs ? lhs : rhs;
}

template <typename T>
inline bool is_power_of_two(T x) {
  return x && !(x & (x - 1));
}