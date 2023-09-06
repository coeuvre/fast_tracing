#pragma once

#include "src/buf.h"
#include "src/defs.h"
#include "src/memory.h"

enum JsonTokenType {
    JsonToken_Unknown,
    JsonToken_Error,
    JsonToken_Eof,

    JsonToken_String,
    JsonToken_Number,

    JsonToken_ObjectStart,
    JsonToken_ObjectEnd,

    JsonToken_ArrayStart,
    JsonToken_ArrayEnd,

    JsonToken_Colon,
    JsonToken_Comma,

    JsonToken_True,
    JsonToken_False,
    JsonToken_Null,
};

struct JsonToken {
    JsonTokenType type;
    Buf value;
};

struct JsonTokenizer {
    MemoryArena arena;

    Buf buf;
    usize buf_cursor;
    u8 state;

    Buf input;
    usize cursor;
    bool last_input;
};

JsonTokenizer json_init_tok();
void json_deinit_tok(JsonTokenizer *tok);

bool json_is_scanning(JsonTokenizer *tok);
void json_set_input(JsonTokenizer *tok, Buf input, bool last_input);

JsonToken json_get_next_token(JsonTokenizer *tok);
