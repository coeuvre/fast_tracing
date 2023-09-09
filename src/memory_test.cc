#include "src/memory.h"

#include <gtest/gtest.h>

TEST(MemoryArenaTest, SingleBlock) {
    MemoryArena arena;
    memory_arena_init(&arena);
    ASSERT_EQ(arena.num_blocks, 0);

    void *data = memory_arena_alloc(&arena, 1);
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(arena.num_blocks, 1);

    memory_arena_free(&arena, data);
    ASSERT_EQ(arena.num_blocks, 1);
    ASSERT_EQ(arena.current, nullptr);

    memory_arena_deinit(&arena);
}

TEST(MemoryArenaTest, MultipleBlocks) {
    MemoryArena arena;
    memory_arena_init(&arena);

    memory_arena_alloc(&arena, 1);

    void *data = memory_arena_alloc(&arena, arena.min_block_size + 1);
    ASSERT_EQ(arena.num_blocks, 2);
    ASSERT_NE(arena.current, arena.head);
    ASSERT_EQ(arena.current, arena.tail);
    ASSERT_GT(arena.current->size, arena.min_block_size + 1);
    ASSERT_LE(arena.current->cursor, arena.current->size);

    memory_arena_free(&arena, data);
    ASSERT_EQ(arena.num_blocks, 2);
    ASSERT_EQ(arena.current, arena.head);

    memory_arena_deinit(&arena);
}

TEST(MemoryArenaTest, ReuseData) {
    MemoryArena arena;
    memory_arena_init(&arena);

    void *data = memory_arena_alloc(&arena, 1);
    void *new_data = memory_arena_realloc(&arena, data, 2);

    ASSERT_EQ(data, new_data);

    memory_arena_deinit(&arena);
}

TEST(MemoryArenaTest, PopAndPushNewBlock) {
    MemoryArena arena;
    memory_arena_init(&arena);

    void *data = memory_arena_alloc(&arena, arena.min_block_size);
    ((u8 *)data)[0] = 0xCC;
    void *new_data =
        memory_arena_realloc(&arena, data, arena.min_block_size << 1);

    ASSERT_NE(data, new_data);
    ASSERT_EQ(((u8 *)new_data)[0], 0xCC);
    ASSERT_EQ(arena.num_blocks, 2);
    ASSERT_EQ(arena.current, arena.tail);
    ASSERT_EQ(arena.head->cursor, sizeof(MemoryBlock));

    memory_arena_deinit(&arena);
}
