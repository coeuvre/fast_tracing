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

MemoryArena memory_arena_init();
void memory_arena_deinit(MemoryArena *arena);

void *memory_arena_push(MemoryArena *arena, usize size);
// Like realloc, reuses the same pointer and extends its capacity if possible
void *memory_arena_push(MemoryArena *arena, void *data, usize new_size);
void memory_arena_pop(MemoryArena *arena, void *data);
void memory_arena_free(MemoryArena *arena, void *data);

void memory_arena_clear(MemoryArena *arena);
