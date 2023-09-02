#ifndef FAST_TRACING_SRC_JSON_H
#define FAST_TRACING_SRC_JSON_H

#include "src/common.h"

enum JsonTokenType {
  kJsonTokenUnknown,
  kJsonTokenError,
  kJsonTokenEof,

  kJsonTokenString,
  kJsonTokenNumber,

  kJsonTokenObjectStart,
  kJsonTokenObjectEnd,

  kJsonTokenArrayStart,
  kJsonTokenArrayEnd,

  kJsonTokenTrue,
  kJsonTokenFalse,
  kJsonTokenNull,
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
};

JsonTokenizer InitJsonTokenizer();
void DeinitJsonTokenizer(JsonTokenizer *tok);

bool IsJsonTokenizerScanning(JsonTokenizer *tok);
void SetJsonTokenizerInput(JsonTokenizer *tok, Buf input);

JsonToken GetNextJsonToken(JsonTokenizer *tok);

#endif  // FAST_TRACING_SRC_JSON_H