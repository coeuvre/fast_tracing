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
  usize offset;
  usize size;
};

static MemoryBlock *PushBlock(MemoryArena *arena, usize block_size) {
  MemoryBlock *block = (MemoryBlock *)malloc(sizeof(MemoryBlock) + block_size);
  ASSERT(block);
  block->size = block_size;
  block->cursor = 0;

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
  MemoryBlock *block = arena->current;

  while (block) {
    if (block->cursor + size <= block->size) {
      break;
    }
    block = block->next;
  }

  if (!block) {
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

void *PushMemory(MemoryArena *arena, void *data, usize new_size) {
  if (!data) {
    return PushMemory(arena, new_size);
  }

  MemoryHeader *header = (MemoryHeader *)data - 1;
  MemoryBlock *block =
      (MemoryBlock *)((u8 *)header - header->offset - sizeof(MemoryBlock));

  usize new_total_size = sizeof(MemoryHeader) + new_size;
  // data is the last allocation in the arena
  if (arena->current == block &&
      header->offset + header->size == block->cursor) {
    if (header->offset + new_total_size <= block->size) {
      block->cursor = header->offset + new_total_size;
      header->size = new_total_size;
      return data;
    }

    // Remaining space in the block is not enough, fall back to new allocation
    PopMemory(arena, data);
  }

  void *new_data = PushMemory(arena, new_size);
  memcpy(new_data, data, header->size - sizeof(MemoryHeader));
  return new_data;
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

  if (block->cursor == 0 && block->prev) {
    arena->current = block->prev;
  }
}

void ClearMemoryArena(MemoryArena *arena) {
  MemoryBlock *block = arena->head;
  while (block) {
    block->cursor = 0;
    block = block->next;
  }
  arena->current = arena->head;
}