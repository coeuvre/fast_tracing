#include "src/memory.h"

#include <memory.h>
#include <stdlib.h>

struct MemoryHeader {
    usize prev;
    // sizeof(MemoryHeader) + sizeof(data)
    usize size;
};

static inline MemoryHeader *get_header(MemoryBlock *block, usize offset) {
    ASSERT(offset >= sizeof(MemoryBlock) && offset < block->size);
    return (MemoryHeader *)((u8 *)block + offset);
}

static MemoryBlock *push_block(MemoryArena *arena, usize block_size) {
    MemoryBlock *block = (MemoryBlock *)malloc(block_size);
    ASSERT(block);
    block->size = block_size;
    block->cursor = sizeof(MemoryBlock);
    MemoryHeader *header = get_header(block, block->cursor);
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

static const usize MIN_BLOCK_SIZE = 4096;

MemoryArena memory_arena_init() {
    MemoryArena arena = {};
    arena.min_block_size = MIN_BLOCK_SIZE;
    return arena;
}

void memory_arena_deinit(MemoryArena *arena) {
    MemoryBlock *block = arena->head;
    while (block) {
        MemoryBlock *next = block->next;
        free(block);
        block = next;
    }
    *arena = {};
}

static void ensure_current_block_size(MemoryArena *arena, usize size) {
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
        ASSERT(is_power_of_two(block_size));
        block = push_block(arena, block_size);
    }

    arena->current = block;
}

void *memory_arena_push(MemoryArena *arena, usize size) {
    ASSERT(size > 0);

    usize total_size = sizeof(MemoryHeader) + size;
    ensure_current_block_size(arena, total_size);

    MemoryBlock *block = arena->current;
    ASSERT(block && block->cursor + total_size <= block->size);

    MemoryHeader *header = get_header(block, block->cursor);
    header->size = total_size;

    MemoryHeader *next_header = get_header(block, block->cursor + total_size);
    next_header->prev = block->cursor;
    next_header->size = 0;

    block->cursor += total_size;

    void *data = (void *)(header + 1);
    memset(data, 0, size);
    return data;
}

void *memory_arena_push(MemoryArena *arena, void *data, usize new_size) {
    if (!data) {
        return memory_arena_push(arena, new_size);
    }

    MemoryHeader *header = (MemoryHeader *)data - 1;
    usize total_size = header->size;

    memory_arena_free(arena, data);
    void *new_data = memory_arena_push(arena, new_size);
    if (new_data != data) {
        memcpy(new_data, data, total_size - sizeof(MemoryHeader));
    }
    return new_data;
}

void memory_arena_pop(MemoryArena *arena, void *data) {
    ASSERT(data);
    ASSERT(arena->current);

    MemoryHeader *header = (MemoryHeader *)data - 1;
    MemoryBlock *block = arena->current;
    MemoryHeader *next_header = get_header(block, block->cursor);

    ASSERT((u8 *)header + header->size == (u8 *)next_header &&
           "Current allocation must be the top one");

    memory_arena_free(arena, data);
}

static void maybe_shrink(MemoryArena *arena) {
    while (arena->current) {
        MemoryBlock *block = arena->current;
        MemoryHeader *header = get_header(block, block->cursor);
        MemoryHeader *prev_header = get_header(block, header->prev);
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

void memory_arena_free(MemoryArena *arena, void *data) {
    if (!data) {
        return;
    }

    MemoryHeader *header = (MemoryHeader *)data - 1;
    header->size = 0;

    maybe_shrink(arena);
}

void memory_arena_clear(MemoryArena *arena) {
    MemoryBlock *block = arena->head;
    while (block) {
        block->cursor = sizeof(MemoryBlock);
        block = block->next;
    }
    arena->current = arena->head;
}