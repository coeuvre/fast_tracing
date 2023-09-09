#pragma once

#include "src/defs.h"

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

void memory_arena_init(MemoryArena *arena);
void memory_arena_deinit(MemoryArena *arena);

void *memory_arena_alloc(MemoryArena *arena, usize size);
void *memory_arena_realloc(MemoryArena *arena, void *data, usize new_size);
void memory_arena_free(MemoryArena *arena, void *data);

void memory_arena_clear(MemoryArena *arena);
