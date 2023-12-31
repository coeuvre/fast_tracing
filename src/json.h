#pragma once

#include "src/buf.h"
#include "src/defs.h"
#include "src/memory.h"

enum JsonTokenType {
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

struct JsonError {
    bool has_error;
    Buf message;
};

struct JsonInput {
    Buf buf;
    usize cursor;
};

void json_input_init(JsonInput *input, Buf buf);

struct JsonToken {
    JsonTokenType type;
    Buf value;
};

// Returns false if there is no more token or an error occurred
bool json_scan(MemoryArena *arena, JsonInput *input, JsonToken *token,
               JsonError *error);
