#include "src/common.h"

#include <gtest/gtest.h>

TEST(MemoryArenaTest, SingleBlock) {
  MemoryArena arena = CreateMemoryArena();
  ASSERT_EQ(arena.num_blocks, 0);

  void *data = PushMemory(&arena, 1);
  ASSERT_NE(data, nullptr);
  ASSERT_EQ(arena.num_blocks, 1);

  PopMemory(&arena, data);
  ASSERT_EQ(arena.num_blocks, 1);
  ASSERT_EQ(arena.current->cursor, 0);

  DestroyMemoryArena(&arena);
}

TEST(MemoryArenaTest, MultipleBlocks) {
  MemoryArena arena = CreateMemoryArena();

  PushMemory(&arena, 1);

  void *data = PushMemory(&arena, arena.min_block_size + 1);
  ASSERT_EQ(arena.num_blocks, 2);
  ASSERT_NE(arena.current, arena.sentinel.next);
  ASSERT_EQ(arena.current, arena.sentinel.prev);
  ASSERT_GT(arena.current->size, arena.min_block_size + 1);
  ASSERT_EQ(arena.current->cursor, arena.current->size);

  PopMemory(&arena, data);
  ASSERT_EQ(arena.num_blocks, 2);
  ASSERT_EQ(arena.current, arena.sentinel.next);

  DestroyMemoryArena(&arena);
}

TEST(MemoryArenaDeathTest, OutOfOrderPop) {
  MemoryArena arena = CreateMemoryArena();

  void *first = PushMemory(&arena, 1);
  PushMemory(&arena, 1);

  ASSERT_DEATH({ PopMemory(&arena, first); },
               "Current allocation must be the top one");
}