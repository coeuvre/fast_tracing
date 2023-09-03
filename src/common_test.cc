#include "src/common.h"

#include <gtest/gtest.h>

TEST(MemoryArenaTest, SingleBlock) {
  MemoryArena arena = InitMemoryArena();
  ASSERT_EQ(arena.num_blocks, 0);

  void *data = PushMemory(&arena, 1);
  ASSERT_NE(data, nullptr);
  ASSERT_EQ(arena.num_blocks, 1);

  PopMemory(&arena, data);
  ASSERT_EQ(arena.num_blocks, 1);
  ASSERT_EQ(arena.current, nullptr);

  DeinitMemoryArena(&arena);
}

TEST(MemoryArenaTest, MultipleBlocks) {
  MemoryArena arena = InitMemoryArena();

  PushMemory(&arena, 1);

  void *data = PushMemory(&arena, arena.min_block_size + 1);
  ASSERT_EQ(arena.num_blocks, 2);
  ASSERT_NE(arena.current, arena.head);
  ASSERT_EQ(arena.current, arena.tail);
  ASSERT_GT(arena.current->size, arena.min_block_size + 1);
  ASSERT_LE(arena.current->cursor, arena.current->size);

  PopMemory(&arena, data);
  ASSERT_EQ(arena.num_blocks, 2);
  ASSERT_EQ(arena.current, arena.head);

  DeinitMemoryArena(&arena);
}

TEST(MemoryArenaTest, ReuseData) {
  MemoryArena arena = InitMemoryArena();

  void *data = PushMemory(&arena, 1);
  void *new_data = PushMemory(&arena, data, 2);

  ASSERT_EQ(data, new_data);

  DeinitMemoryArena(&arena);
}

TEST(MemoryArenaTest, PopAndPushNewBlock) {
  MemoryArena arena = InitMemoryArena();

  void *data = PushMemory(&arena, arena.min_block_size);
  ((u8 *)data)[0] = 0xCC;
  void *new_data = PushMemory(&arena, data, arena.min_block_size << 1);

  ASSERT_NE(data, new_data);
  ASSERT_EQ(((u8 *)new_data)[0], 0xCC);
  ASSERT_EQ(arena.num_blocks, 2);
  ASSERT_EQ(arena.current, arena.tail);
  ASSERT_EQ(arena.head->cursor, sizeof(MemoryBlock));

  DeinitMemoryArena(&arena);
}

TEST(MemoryArenaDeathTest, OutOfOrderPop) {
  MemoryArena arena = InitMemoryArena();

  void *first = PushMemory(&arena, 1);
  PushMemory(&arena, 1);

  ASSERT_DEATH({ PopMemory(&arena, first); },
               "Current allocation must be the top one");
}