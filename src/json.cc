#include "src/json.h"

#include <stdlib.h>

enum {
  kJsonTokenizerStateError,
  kJsonTokenizerStateStart,
  kJsonTokenizerStateString,
  kJsonTokenizerStateStringEscape,
  // kJsonTokenizerStateStringEscapeU0,
  // kJsonTokenizerStateStringEscapeU1,
  // kJsonTokenizerStateStringEscapeU2,
  // kJsonTokenizerStateStringEscapeU3,
  kJsonTokenizerStateStringEnd,
};

JsonTokenizer InitJsonTokenizer() {
  JsonTokenizer tok = {};
  tok.arena = InitMemoryArena();
  tok.state = kJsonTokenizerStateStart;
  return tok;
}

void DeinitJsonTokenizer(JsonTokenizer *tok) {
  DeinitMemoryArena(&tok->arena);
  *tok = {};
}

bool IsJsonTokenizerScanning(JsonTokenizer *tok) {
  return tok->state != kJsonTokenizerStateError;
}

void SetJsonTokenizerInput(JsonTokenizer *tok, Buf input) {
  ASSERT(tok->cursor == tok->input.size && "Last input was not fully consumed");
  tok->input = input;
  tok->cursor = 0;
}

static inline bool HasInput(JsonTokenizer *tok) {
  return tok->cursor < tok->input.size;
}

static inline u8 TakeInput(JsonTokenizer *tok) {
  ASSERT(HasInput(tok));
  return ((u8 *)tok->input.data)[tok->cursor++];
}

static void SkipWhitespace(JsonTokenizer *tok) {
  while (tok->cursor < tok->input.size) {
    u8 c = ((u8 *)tok->input.data)[tok->cursor];
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
      break;
    }
    tok->cursor++;
  }
}

static void OnStart(JsonTokenizer *tok, JsonToken *token) {
  SkipWhitespace(tok);

  if (!HasInput(tok)) {
    *token = {.type = kJsonTokenEof};
    return;
  }

  switch (TakeInput(tok)) {
    case '"': {
      tok->state = kJsonTokenizerStateString;
      ASSERT(tok->buf_cursor == 0);
    } break;

    case '[': {
      *token = {.type = kJsonTokenArrayStart};
    } break;

    case ']': {
      *token = {.type = kJsonTokenArrayEnd};
    } break;

    case '{': {
      *token = {.type = kJsonTokenObjectStart};
    } break;

    case '}': {
      *token = {.type = kJsonTokenObjectEnd};
    } break;

    default: {
      tok->state = kJsonTokenizerStateError;
      *token = {.type = kJsonTokenError,
                .value = STR_LITERAL("Unexpected character")};
    } break;
  }
}

const usize kJsonTokenizerBufInitialSize = 1024;

static inline void SaveChar(JsonTokenizer *tok, u8 ch) {
  // TODO: Grow buffer
  ASSERT(tok->buf_cursor <= tok->buf.size);

  while ((tok->buf_cursor + 1) >= tok->buf.size) {
    tok->buf.size = Max(kJsonTokenizerBufInitialSize, tok->buf.size * 2);
    tok->buf.data = PushMemory(&tok->arena, tok->buf.data, tok->buf.size);
  }

  ASSERT((tok->buf_cursor + 1) < tok->buf.size);
  ((u8 *)tok->buf.data)[tok->buf_cursor++] = ch;
  ((u8 *)tok->buf.data)[tok->buf_cursor] = 0;
}

static void OnString(JsonTokenizer *tok, JsonToken *token) {
  while (token->type == kJsonTokenUnknown) {
    if (!HasInput(tok)) {
      *token = {.type = kJsonTokenEof};
      return;
    }

    u8 ch = TakeInput(tok);
    switch (ch) {
      case '"': {
        tok->state = kJsonTokenizerStateStringEnd;
        *token = {.type = kJsonTokenString,
                  .value = GetSubBuf(tok->buf, 0, tok->buf_cursor)};
      } break;

      case '\\': {
        tok->state = kJsonTokenizerStateStringEscape;
        return;
      } break;

      default: {
        SaveChar(tok, ch);
      } break;
    }
  }
}

static void OnStringEscape(JsonTokenizer *tok, JsonToken *token) {
  if (!HasInput(tok)) {
    *token = {.type = kJsonTokenEof};
    return;
  }

  u8 ch = TakeInput(tok);
  switch (ch) {
    case '"':
    case '\\':
    case '/': {
      SaveChar(tok, ch);
      tok->state = kJsonTokenizerStateString;
    } break;

    case 'b': {
      SaveChar(tok, '\b');
      tok->state = kJsonTokenizerStateString;
    } break;

    case 'f': {
      SaveChar(tok, '\f');
      tok->state = kJsonTokenizerStateString;
    } break;

    case 'n': {
      SaveChar(tok, '\n');
      tok->state = kJsonTokenizerStateString;
    } break;

    case 'r': {
      SaveChar(tok, '\r');
      tok->state = kJsonTokenizerStateString;
    } break;

    case 't': {
      SaveChar(tok, '\t');
      tok->state = kJsonTokenizerStateString;
    } break;

      // TODO: Unicode escapes
      // case 'u': {
      //   tok->state = kJsonTokenizerStateStringEscapeU0;
      // } break;

    default: {
      tok->state = kJsonTokenizerStateError;
      *token = {.type = kJsonTokenError,
                .value = STR_LITERAL("Unexpected escape character")};
    } break;
  }
}

// static bool MaybeSaveHex(JsonTokenizer *tok, u8 *out) {
//   u8 ch = TakeInput(tok);
//   switch (ch) {
//     case '0':
//     case '1':
//     case '2':
//     case '3':
//     case '4':
//     case '5':
//     case '6':
//     case '7':
//     case '8':
//     case '9':
//     case 'a':
//     case 'b':
//     case 'c':
//     case 'd':
//     case 'e':
//     case 'f':
//     case 'A':
//     case 'B':
//     case 'C':
//     case 'D':
//     case 'E':
//     case 'F': {
//       SaveChar(tok, ch);

//       *out = ch;
//       return true;
//     } break;

//     default: {
//       *out = ch;
//       return false;
//     } break;
//   }
// }

// static void OnStringEscapeU0(JsonTokenizer *tok, JsonToken *token) {
//   if (!HasInput(tok)) {
//     *token = {.type = kJsonTokenEof};
//     return;
//   }

//   u8 ch;
//   if (MaybeSaveHex(tok, &ch)) {
//     tok->state = kJsonTokenizerStateStringEscapeU1;
//   } else {
//     tok->state = kJsonTokenizerStateError;
//     *token = {.type = kJsonTokenError,
//               .value = STR_LITERAL("Expected hex digit")};
//   }
// }

// static void OnStringEscape1(JsonTokenizer *tok, JsonToken *token) {
//   if (!HasInput(tok)) {
//     *token = {.type = kJsonTokenEof};
//     return;
//   }

//   u8 ch;
//   if (MaybeSaveHex(tok, &ch)) {
//     tok->state = kJsonTokenizerStateStringEscapeU2;
//   } else {
//     tok->state = kJsonTokenizerStateError;
//     *token = {.type = kJsonTokenError,
//               .value = STR_LITERAL("Expected hex digit")};
//   }
// }

// static void OnStringEscape2(JsonTokenizer *tok, JsonToken *token) {
//   if (!HasInput(tok)) {
//     *token = {.type = kJsonTokenEof};
//     return;
//   }

//   u8 ch;
//   if (MaybeSaveHex(tok, &ch)) {
//     tok->state = kJsonTokenizerStateStringEscapeU3;
//   } else {
//     tok->state = kJsonTokenizerStateError;
//     *token = {.type = kJsonTokenError,
//               .value = STR_LITERAL("Expected hex digit")};
//   }
// }

// static void OnStringEscape3(JsonTokenizer *tok, JsonToken *token) {
//   if (!HasInput(tok)) {
//     *token = {.type = kJsonTokenEof};
//     return;
//   }

//   u8 ch;
//   if (MaybeSaveHex(tok, &ch)) {
//     tok->buf.data
//     tok->buf_cursor - 4;
//     tok->state = kJsonTokenizerStateString;
//   } else {
//     tok->state = kJsonTokenizerStateError;
//     *token = {.type = kJsonTokenError,
//               .value = STR_LITERAL("Expected hex digit")};
//   }
// }

static void OnStringEnd(JsonTokenizer *tok, JsonToken *token) {
  tok->buf_cursor = 0;
  tok->state = kJsonTokenizerStateStart;
}

JsonToken GetNextJsonToken(JsonTokenizer *tok) {
  ASSERT(tok->cursor <= tok->input.size && "No more input to process");
  ASSERT(IsJsonTokenizerScanning(tok));

  JsonToken token = {.type = kJsonTokenUnknown};
  while (token.type == kJsonTokenUnknown) {
    switch (tok->state) {
      case kJsonTokenizerStateStart: {
        OnStart(tok, &token);
      } break;

      case kJsonTokenizerStateString: {
        OnString(tok, &token);
      } break;

      case kJsonTokenizerStateStringEscape: {
        OnStringEscape(tok, &token);
      } break;

      case kJsonTokenizerStateStringEnd: {
        OnStringEnd(tok, &token);
      } break;

      default:
        UNREACHABLE;
    }
  }
  return token;
}
