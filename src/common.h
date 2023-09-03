#ifndef FAST_TRACING_SRC_COMMON_H_
#define FAST_TRACING_SRC_COMMON_H_

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
#define UNREACHABLE ASSERT("unreachable" && false)

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
inline T Min(T lhs, T rhs) {
  return lhs < rhs ? lhs : rhs;
}

template <typename T>
inline T Max(T lhs, T rhs) {
  return lhs > rhs ? lhs : rhs;
}

template <typename T>
inline bool IsPowerOfTwo(T x) {
  return x && !(x & (x - 1));
}

struct Buf {
  void *data;
  usize size;
};

bool BufAreEqual(Buf lhs, Buf rhs);
Buf GetSubBuf(Buf buf, usize start, usize end);

#define STR_LITERAL(s) \
  Buf { .data = (void *)s, .size = sizeof(s) - 1 }

struct MemoryBlock {
  MemoryBlock *prev;
  MemoryBlock *next;
  // Total allocated size, including this header
  usize size;
  usize cursor;
};

struct MemoryArena {
  MemoryBlock *head;
  MemoryBlock *tail;
  MemoryBlock *current;
  usize min_block_size;
  usize num_blocks;
};

MemoryArena InitMemoryArena();
void DeinitMemoryArena(MemoryArena *arena);

void *PushMemory(MemoryArena *arena, usize size);
// Like realloc, reuses the same pointer and extends its capacity if possible
void *PushMemory(MemoryArena *arena, void *data, usize new_size);
void PopMemory(MemoryArena *arena, void *data);
void FreeMemory(MemoryArena *arena, void *data);

void ClearMemoryArena(MemoryArena *arena);

#endif  // FAST_TRACING_SRC_COMMON_H_
