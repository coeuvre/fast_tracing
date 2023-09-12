#pragma once

#include "src/buf.h"
#include "src/defs.h"
#include "src/memory.h"
#include "src/trace.h"

struct JsonTraceParserState_ObjectFormat {
    usize buf_cursor;
};

struct JsonTraceParserState_SkipChar {
    u8 target;
    u8 next_state;
};

struct JsonTraceParserState_UnknownKey {
    usize stack_cursor;
    u8 last_char;
};

struct JsonTraceParser {
    MemoryArena *arena;
    Buf buf;
    u8 state;
    union {
        JsonTraceParserState_ObjectFormat object_format;
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
