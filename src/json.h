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

  kJsonTokenColon,
  kJsonTokenComma,

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
  bool last_input;
};

JsonTokenizer InitJsonTokenizer();
void DeinitJsonTokenizer(JsonTokenizer *tok);

bool IsJsonTokenizerScanning(JsonTokenizer *tok);
void SetJsonTokenizerInput(JsonTokenizer *tok, Buf input, bool last_input);

JsonToken GetNextJsonToken(JsonTokenizer *tok);

#endif  // FAST_TRACING_SRC_JSON_H