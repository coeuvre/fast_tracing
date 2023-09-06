#include "src/memory.h"

#include <gtest/gtest.h>

TEST(MemoryArenaTest, SingleBlock) {
    MemoryArena arena = memory_arena_init();
    ASSERT_EQ(arena.num_blocks, 0);

    void *data = memory_arena_push(&arena, 1);
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(arena.num_blocks, 1);

    memory_arena_pop(&arena, data);
    ASSERT_EQ(arena.num_blocks, 1);
    ASSERT_EQ(arena.current, nullptr);

    memory_arena_deinit(&arena);
}

TEST(MemoryArenaTest, MultipleBlocks) {
    MemoryArena arena = memory_arena_init();

    memory_arena_push(&arena, 1);

    void *data = memory_arena_push(&arena, arena.min_block_size + 1);
    ASSERT_EQ(arena.num_blocks, 2);
    ASSERT_NE(arena.current, arena.head);
    ASSERT_EQ(arena.current, arena.tail);
    ASSERT_GT(arena.current->size, arena.min_block_size + 1);
    ASSERT_LE(arena.current->cursor, arena.current->size);

    memory_arena_pop(&arena, data);
    ASSERT_EQ(arena.num_blocks, 2);
    ASSERT_EQ(arena.current, arena.head);

    memory_arena_deinit(&arena);
}

TEST(MemoryArenaTest, ReuseData) {
    MemoryArena arena = memory_arena_init();

    void *data = memory_arena_push(&arena, 1);
    void *new_data = memory_arena_push(&arena, data, 2);

    ASSERT_EQ(data, new_data);

    memory_arena_deinit(&arena);
}

TEST(MemoryArenaTest, PopAndPushNewBlock) {
    MemoryArena arena = memory_arena_init();

    void *data = memory_arena_push(&arena, arena.min_block_size);
    ((u8 *)data)[0] = 0xCC;
    void *new_data = memory_arena_push(&arena, data, arena.min_block_size << 1);

    ASSERT_NE(data, new_data);
    ASSERT_EQ(((u8 *)new_data)[0], 0xCC);
    ASSERT_EQ(arena.num_blocks, 2);
    ASSERT_EQ(arena.current, arena.tail);
    ASSERT_EQ(arena.head->cursor, sizeof(MemoryBlock));

    memory_arena_deinit(&arena);
}

TEST(MemoryArenaDeathTest, OutOfOrderPop) {
    MemoryArena arena = memory_arena_init();

    void *first = memory_arena_push(&arena, 1);
    memory_arena_push(&arena, 1);

    ASSERT_DEATH({ memory_arena_pop(&arena, first); },
                 "Current allocation must be the top one");
}