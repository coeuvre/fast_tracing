#pragma once

#include "src/buf.h"
#include "src/defs.h"
#include "src/memory.h"
#include "src/trace.h"

struct JsonTraceParserState_ArrayFormat {
    u8 last_char;
};

struct JsonTraceParserState_SkipChar {
    u8 target;
    u8 next_state;
};

struct JsonTraceParserState_UnknownKey {
    bool init;
    u8 last_char;
};

struct JsonTraceParser {
    MemoryArena *arena;
    Buf buf;
    usize buf_cursor;
    Buf stack;
    usize stack_cursor;
    bool has_object_format;
    u8 state;
    union {
        JsonTraceParserState_ArrayFormat array_format;
        JsonTraceParserState_SkipChar skip_char;
        JsonTraceParserState_UnknownKey unknown_key;
    };
};

enum JsonTraceResult {
    JsonTraceResult_Error,
    JsonTraceResult_Done,
    JsonTraceResult_NeedMoreInput,
};

void json_trace_parser_init(JsonTraceParser *parser, MemoryArena *arena);
void json_trace_parser_deinit(JsonTraceParser *parser);

JsonTraceResult json_trace_parser_parse(JsonTraceParser *parser, Trace *trace,
                                        Buf buf);
char *json_trace_parser_get_error(JsonTraceParser *parser);
