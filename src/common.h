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

inline usize Min(usize lhs, usize rhs) { return lhs < rhs ? lhs : rhs; }

inline usize Max(usize lhs, usize rhs) { return lhs > rhs ? lhs : rhs; }

struct Buf {
  u8 *ptr;
  usize len;
};

bool BufEql(Buf lhs, Buf rhs);
Buf BufSub(Buf buf, usize start, usize end);

#define STR_LITERAL(s) \
  Buf { .ptr = (u8 *)s, .len = sizeof(s) - 1 }

struct MemoryBlock {
  MemoryBlock *prev;
  MemoryBlock *next;
  usize size;
  usize cursor;
};

struct MemoryArena {
  MemoryBlock sentinel;
  MemoryBlock *current;
  usize min_block_size;
  usize num_blocks;
};

MemoryArena CreateMemoryArena();
void DestroyMemoryArena(MemoryArena *arena);

void *PushMemory(MemoryArena *arena, usize size);
void PopMemory(MemoryArena *arena, void *data);

#endif  // FAST_TRACING_SRC_COMMON_H_
