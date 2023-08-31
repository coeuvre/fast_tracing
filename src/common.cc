#include "src/common.h"

#include <memory.h>
#include <stdlib.h>

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

Buf BufSub(Buf buf, usize start, usize end) {
  ASSERT(start <= end);
  ASSERT(end <= buf.len);
  return Buf{.ptr = buf.ptr + start, .len = end - start};
}

struct MemoryHeader {
  usize offset;
  usize size;
};

static MemoryBlock *PushBlock(MemoryArena *arena, usize block_size) {
  MemoryBlock *block = (MemoryBlock *)malloc(sizeof(MemoryBlock) + block_size);
  ASSERT(block);
  block->size = block_size;
  block->cursor = 0;

  block->prev = arena->sentinel.prev;
  arena->sentinel.prev->next = block;

  block->next = &arena->sentinel;
  arena->sentinel.prev = block;

  arena->num_blocks++;

  return block;
}

static const usize kArenaMinBlockSize = 4096;

MemoryArena CreateMemoryArena() {
  MemoryArena arena = {};
  arena.sentinel.prev = &arena.sentinel;
  arena.sentinel.next = &arena.sentinel;
  arena.current = &arena.sentinel;
  arena.min_block_size = kArenaMinBlockSize;
  return arena;
}

void DestroyMemoryArena(MemoryArena *arena) {
  MemoryBlock *block = arena->sentinel.next;
  while (block != &arena->sentinel) {
    MemoryBlock *next = block->next;
    free(block);
    block = next;
  }
  arena->sentinel.prev = &arena->sentinel;
  arena->sentinel.next = &arena->sentinel;
  arena->current = &arena->sentinel;
  arena->num_blocks = 0;
}

static void EnsureCurrentBlock(MemoryArena *arena, usize size) {
  MemoryBlock *block = arena->current;

  while (block != &arena->sentinel) {
    if (block->cursor + size <= block->size) {
      break;
    }
    block = block->next;
  }

  if (block == &arena->sentinel) {
    block = PushBlock(arena, Max(size, arena->min_block_size));
  }

  arena->current = block;
}

void *PushMemory(MemoryArena *arena, usize size) {
  ASSERT(size > 0);

  usize total_size = sizeof(MemoryHeader) + size;
  EnsureCurrentBlock(arena, total_size);

  MemoryBlock *block = arena->current;
  ASSERT(block && block->cursor + total_size <= block->size);

  MemoryHeader *header =
      (MemoryHeader *)((u8 *)(block + 1) + block->cursor);
  header->offset = block->cursor;
  header->size = total_size;

  block->cursor += total_size;

  void *data = (void *)(header + 1);
  memset(data, 0, size);
  return data;
}

void PopMemory(MemoryArena *arena, void *data) {
  ASSERT(data);
  MemoryHeader *header = (MemoryHeader *)data - 1;
  MemoryBlock *block =
      (MemoryBlock *)((u8 *)header - header->offset - sizeof(MemoryBlock));

  ASSERT(arena->current == block);
  ASSERT(header->offset + header->size == block->cursor &&
         "Current allocation must be the top one");

  block->cursor = header->offset;

  if (block->cursor == 0 && block->prev != &arena->sentinel) {
    arena->current = block->prev;
  }
}