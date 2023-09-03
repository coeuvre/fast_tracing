#include "src/common.h"

#include <memory.h>
#include <stdlib.h>

bool BufAreEqual(Buf lhs, Buf rhs) {
  if (lhs.size != rhs.size) {
    return false;
  }

  u8 *a = (u8 *)lhs.data;
  u8 *b = (u8 *)rhs.data;

  for (usize i = 0; i < lhs.size; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }

  return true;
}

Buf GetSubBuf(Buf buf, usize start, usize end) {
  ASSERT(start <= end);
  ASSERT(end <= buf.size);
  return Buf{.data = (u8 *)buf.data + start, .size = end - start};
}

struct MemoryHeader {
  usize prev;
  // sizeof(MemoryHeader) + sizeof(data)
  usize size;
};

static inline MemoryHeader *GetHeader(MemoryBlock *block, usize offset) {
  ASSERT(offset >= sizeof(MemoryBlock) && offset < block->size);
  return (MemoryHeader *)((u8 *)block + offset);
}

static MemoryBlock *PushBlock(MemoryArena *arena, usize block_size) {
  MemoryBlock *block = (MemoryBlock *)malloc(block_size);
  ASSERT(block);
  block->size = block_size;
  block->cursor = sizeof(MemoryBlock);
  MemoryHeader *header = GetHeader(block, block->cursor);
  header->prev = block->cursor;

  block->next = 0;
  block->prev = arena->tail;
  arena->tail = block;
  if (!arena->head) {
    arena->head = block;
  }

  arena->num_blocks++;

  return block;
}

static const usize kArenaMinBlockSize = 4096;

MemoryArena InitMemoryArena() {
  MemoryArena arena = {};
  arena.min_block_size = kArenaMinBlockSize;
  return arena;
}

void DeinitMemoryArena(MemoryArena *arena) {
  MemoryBlock *block = arena->head;
  while (block) {
    MemoryBlock *next = block->next;
    free(block);
    block = next;
  }
  *arena = {};
}

static void EnsureCurrentBlock(MemoryArena *arena, usize size) {
  if (!arena->current) {
    arena->current = arena->head;
  }

  MemoryBlock *block = arena->current;
  while (block) {
    if (block->cursor + size <= block->size) {
      break;
    }
    block = block->next;
  }

  if (!block) {
    usize block_size = arena->min_block_size;
    if (block && block->prev) {
      block_size = block->prev->size << 1;
    }
    while ((block_size - sizeof(MemoryBlock)) <= size) {
      block_size <<= 1;
    }
    ASSERT(IsPowerOfTwo(block_size));
    block = PushBlock(arena, block_size);
  }

  arena->current = block;
}

void *PushMemory(MemoryArena *arena, usize size) {
  ASSERT(size > 0);

  usize total_size = sizeof(MemoryHeader) + size;
  EnsureCurrentBlock(arena, total_size);

  MemoryBlock *block = arena->current;
  ASSERT(block && block->cursor + total_size <= block->size);

  MemoryHeader *header = GetHeader(block, block->cursor);
  header->size = total_size;

  MemoryHeader *next_header = GetHeader(block, block->cursor + total_size);
  next_header->prev = block->cursor;
  next_header->size = 0;

  block->cursor += total_size;

  void *data = (void *)(header + 1);
  memset(data, 0, size);
  return data;
}

void *PushMemory(MemoryArena *arena, void *data, usize new_size) {
  if (!data) {
    return PushMemory(arena, new_size);
  }

  MemoryHeader *header = (MemoryHeader *)data - 1;
  usize total_size = header->size;

  FreeMemory(arena, data);
  void *new_data = PushMemory(arena, new_size);
  if (new_data != data) {
    memcpy(new_data, data, total_size - sizeof(MemoryHeader));
  }
  return new_data;
}

void PopMemory(MemoryArena *arena, void *data) {
  ASSERT(data);
  ASSERT(arena->current);

  MemoryHeader *header = (MemoryHeader *)data - 1;
  MemoryBlock *block = arena->current;
  MemoryHeader *next_header = GetHeader(block, block->cursor);

  ASSERT((u8 *)header + header->size == (u8 *)next_header &&
         "Current allocation must be the top one");

  FreeMemory(arena, data);
}

static void MaybeShrink(MemoryArena *arena) {
  while (arena->current) {
    MemoryBlock *block = arena->current;
    MemoryHeader *header = GetHeader(block, block->cursor);
    MemoryHeader *prev_header = GetHeader(block, header->prev);
    if (prev_header->size == 0) {
      block->cursor = header->prev;
      if (block->cursor == sizeof(MemoryBlock)) {
        arena->current = block->prev;
      }
    } else {
      break;
    }
  }
}

void FreeMemory(MemoryArena *arena, void *data) {
  if (!data) {
    return;
  }

  MemoryHeader *header = (MemoryHeader *)data - 1;
  header->size = 0;

  MaybeShrink(arena);
}

void ClearMemoryArena(MemoryArena *arena) {
  MemoryBlock *block = arena->head;
  while (block) {
    block->cursor = sizeof(MemoryBlock);
    block = block->next;
  }
  arena->current = arena->head;
}