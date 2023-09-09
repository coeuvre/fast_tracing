#pragma once

#include <stdlib.h>

#include "src/defs.h"

#define memory_alloc(size) malloc(size)
#define memory_realloc(data, new_size) realloc(data, new_size)
#define memory_calloc(count, size) calloc(count, size)
#define memory_free(data) free(data)

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
