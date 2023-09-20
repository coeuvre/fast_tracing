#pragma once

#include "src/buf.h"
#include "src/memory.h"

struct TraceEvent {
    Buf name;
    Buf cat;
    u8 ph;
    u64 ts;
    u32 pid;
    u32 tid;
};

struct Trace {
    MemoryArena arena;
};
