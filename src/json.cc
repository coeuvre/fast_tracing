#include "src/json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const usize kJsonErrorMessageSize = 1024;

enum {
  kJsonTokenizerStateError,
  kJsonTokenizerStateStart,
  kJsonTokenizerStateDone,
  kJsonTokenizerStateString,
  kJsonTokenizerStateStringEscape,
  kJsonTokenizerStateStringEscapeU0,
  kJsonTokenizerStateStringEscapeU1,
  kJsonTokenizerStateStringEscapeU2,
  kJsonTokenizerStateStringEscapeU3,
  kJsonTokenizerStateStringEnd,
  kJsonTokenizerStateInteger,
  kJsonTokenizerStateFraction,
  kJsonTokenizerStateExpoinent,
  kJsonTokenizerStateExpoinentNoSign,
  kJsonTokenizerStateNumberEnd,
  kJsonTokenizerStateT,
  kJsonTokenizerStateTr,
  kJsonTokenizerStateTru,
  kJsonTokenizerStateF,
  kJsonTokenizerStateFa,
  kJsonTokenizerStateFal,
  kJsonTokenizerStateFals,
  kJsonTokenizerStateN,
  kJsonTokenizerStateNu,
  kJsonTokenizerStateNul,
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
  return tok->state != kJsonTokenizerStateError &&
         tok->state != kJsonTokenizerStateDone;
}

void SetJsonTokenizerInput(JsonTokenizer *tok, Buf input, bool last_input) {
  ASSERT(tok->cursor == tok->input.size && "Last input was not fully consumed");
  tok->input = input;
  tok->last_input = last_input;
  tok->cursor = 0;
}

static void SetError(JsonTokenizer *tok, JsonToken *token, const char *fmt,
                     ...) {
  ClearMemoryArena(&tok->arena);

  usize buf_size = kJsonErrorMessageSize;
  char *buf = (char *)PushMemory(&tok->arena, buf_size);

  va_list va;
  va_start(va, fmt);
  usize nwritten = vsnprintf(buf, buf_size, fmt, va);
  va_end(va);

  tok->state = kJsonTokenizerStateError;
  *token = {.type = kJsonTokenError,
            .value = {.data = buf, .size = Min(buf_size, nwritten)}};
}

static inline bool HasInput(JsonTokenizer *tok) {
  return tok->cursor < tok->input.size;
}

static inline u8 TakeInput(JsonTokenizer *tok) {
  ASSERT(HasInput(tok));
  return ((u8 *)tok->input.data)[tok->cursor++];
}

static inline void ReturnInput(JsonTokenizer *tok) {
  ASSERT(tok->cursor > 0);
  tok->cursor--;
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

const usize kJsonTokenizerBufInitialSize = 1024;

static inline void SaveChar(JsonTokenizer *tok, u8 ch) {
  ASSERT(tok->buf_cursor <= tok->buf.size);

  while ((tok->buf_cursor + 1) >= tok->buf.size) {
    tok->buf.size = Max(kJsonTokenizerBufInitialSize, tok->buf.size * 2);
    tok->buf.data = PushMemory(&tok->arena, tok->buf.data, tok->buf.size);
  }

  ASSERT((tok->buf_cursor + 1) < tok->buf.size);
  ((u8 *)tok->buf.data)[tok->buf_cursor++] = ch;
  // Be compatible with C strings
  ((u8 *)tok->buf.data)[tok->buf_cursor] = 0;
}

static void SetEof(JsonTokenizer *tok, JsonToken *token) {
  *token = {.type = kJsonTokenEof};
  if (tok->last_input) {
    tok->state = kJsonTokenizerStateDone;
  }
}

static void OnStart(JsonTokenizer *tok, JsonToken *token) {
  SkipWhitespace(tok);

  if (!HasInput(tok)) {
    SetEof(tok, token);
    return;
  }

  char ch = TakeInput(tok);
  switch (ch) {
    case '"': {
      ASSERT(tok->buf_cursor == 0);
      tok->state = kJsonTokenizerStateString;
    } break;

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      ASSERT(tok->buf_cursor == 0);
      tok->state = kJsonTokenizerStateInteger;
      SaveChar(tok, ch);
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

    case ':': {
      *token = {.type = kJsonTokenColon};
    } break;

    case ',': {
      *token = {.type = kJsonTokenComma};
    } break;

    case 't': {
      tok->state = kJsonTokenizerStateT;
    } break;

    case 'f': {
      tok->state = kJsonTokenizerStateF;
    } break;

    case 'n': {
      tok->state = kJsonTokenizerStateN;
    } break;

    default: {
      tok->state = kJsonTokenizerStateError;
      SetError(tok, token, "JSON value expected but got '%c'", ch);
    } break;
  }
}

static void SetStringErrorOrEof(JsonTokenizer *tok, JsonToken *token) {
  if (tok->last_input) {
    SetError(tok, token,
             "End of string '\"' expected but reached end of input");
  } else {
    SetEof(tok, token);
  }
}

static void OnString(JsonTokenizer *tok, JsonToken *token) {
  while (token->type == kJsonTokenUnknown) {
    if (!HasInput(tok)) {
      SetStringErrorOrEof(tok, token);
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
        SaveChar(tok, ch);
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
    SetStringErrorOrEof(tok, token);
    return;
  }

  u8 ch = TakeInput(tok);
  switch (ch) {
    case '"':
    case '\\':
    case '/':
    case 'b':
    case 'f':
    case 'n':
    case 'r':
    case 't': {
      SaveChar(tok, ch);
      tok->state = kJsonTokenizerStateString;
    } break;

    case 'u': {
      SaveChar(tok, ch);
      tok->state = kJsonTokenizerStateStringEscapeU0;
    } break;

    default: {
      tok->state = kJsonTokenizerStateError;
      SetError(tok, token, "Invalid escape character '\\%c'", ch);
    } break;
  }
}

static void OnStringEscapeU(JsonTokenizer *tok, JsonToken *token,
                            u8 next_state) {
  if (!HasInput(tok)) {
    SetEof(tok, token);
    return;
  }

  u8 ch = TakeInput(tok);
  switch (ch) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F': {
      SaveChar(tok, ch);
      tok->state = next_state;
    } break;

    default: {
      tok->state = kJsonTokenizerStateError;
      SetError(tok, token, "Expected hex digit but got '%c'", ch);
    } break;
  }
}

static void OnStringEnd(JsonTokenizer *tok, JsonToken *token) {
  PopMemory(&tok->arena, tok->buf.data);
  tok->buf.data = 0;
  tok->buf.size = 0;
  tok->buf_cursor = 0;
  tok->state = kJsonTokenizerStateStart;
}

static void SetNumberOrEof(JsonTokenizer *tok, JsonToken *token) {
  if (tok->last_input) {
    tok->state = kJsonTokenizerStateNumberEnd;
    *token = {.type = kJsonTokenNumber,
              .value = GetSubBuf(tok->buf, 0, tok->buf_cursor)};
  } else {
    SetEof(tok, token);
  }
}

static void OnInteger(JsonTokenizer *tok, JsonToken *token) {
  while (token->type == kJsonTokenUnknown) {
    if (!HasInput(tok)) {
      SetNumberOrEof(tok, token);
      return;
    }

    u8 ch = TakeInput(tok);
    switch (ch) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        if (((u8 *)tok->buf.data)[0] == '0' ||
            (((u8 *)tok->buf.data)[0] == '-' && tok->buf_cursor == 2 &&
             ((u8 *)tok->buf.data)[1] == '0')) {
          ReturnInput(tok);
          *token = {.type = kJsonTokenNumber,
                    .value = GetSubBuf(tok->buf, 0, tok->buf_cursor)};
          tok->state = kJsonTokenizerStateStart;
        } else {
          SaveChar(tok, ch);
        }
      } break;

      case '.': {
        SaveChar(tok, ch);
        tok->state = kJsonTokenizerStateFraction;
        return;
      } break;

      case 'e':
      case 'E': {
        SaveChar(tok, ch);
        tok->state = kJsonTokenizerStateExpoinent;
        return;
      } break;

      default: {
        ReturnInput(tok);
        tok->state = kJsonTokenizerStateNumberEnd;
        *token = {.type = kJsonTokenNumber,
                  .value = GetSubBuf(tok->buf, 0, tok->buf_cursor)};
      } break;
    }
  }
}

static void OnFraction(JsonTokenizer *tok, JsonToken *token) {
  while (token->type == kJsonTokenUnknown) {
    if (!HasInput(tok)) {
      SetNumberOrEof(tok, token);
      return;
    }

    u8 ch = TakeInput(tok);
    switch (ch) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        SaveChar(tok, ch);
      } break;

      case 'e':
      case 'E': {
        SaveChar(tok, ch);
        tok->state = kJsonTokenizerStateExpoinent;
        return;
      } break;

      default: {
        ReturnInput(tok);
        tok->state = kJsonTokenizerStateNumberEnd;
        *token = {.type = kJsonTokenNumber,
                  .value = GetSubBuf(tok->buf, 0, tok->buf_cursor)};
      } break;
    }
  }
}

static void OnExpoinent(JsonTokenizer *tok, JsonToken *token) {
  while (token->type == kJsonTokenUnknown) {
    if (!HasInput(tok)) {
      SetNumberOrEof(tok, token);
      return;
    }

    u8 ch = TakeInput(tok);
    switch (ch) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        SaveChar(tok, ch);
      } break;

      case '+':
      case '-': {
        SaveChar(tok, ch);
        tok->state = kJsonTokenizerStateExpoinentNoSign;
        return;
      } break;

      default: {
        ReturnInput(tok);
        tok->state = kJsonTokenizerStateNumberEnd;
        *token = {.type = kJsonTokenNumber,
                  .value = GetSubBuf(tok->buf, 0, tok->buf_cursor)};
      } break;
    }
  }
}

static void OnExpoinentNoSign(JsonTokenizer *tok, JsonToken *token) {
  while (token->type == kJsonTokenUnknown) {
    if (!HasInput(tok)) {
      SetNumberOrEof(tok, token);
      return;
    }

    u8 ch = TakeInput(tok);
    switch (ch) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        SaveChar(tok, ch);
      } break;

      default: {
        ReturnInput(tok);
        tok->state = kJsonTokenizerStateNumberEnd;
        *token = {.type = kJsonTokenNumber,
                  .value = GetSubBuf(tok->buf, 0, tok->buf_cursor)};
      } break;
    }
  }
}

static void OnNumberEnd(JsonTokenizer *tok, JsonToken *token) {
  PopMemory(&tok->arena, tok->buf.data);
  tok->buf.data = 0;
  tok->buf.size = 0;
  tok->buf_cursor = 0;
  tok->state = kJsonTokenizerStateStart;
}

static bool ExpectChar(JsonTokenizer *tok, JsonToken *token, char expected_ch,
                       u8 next_state) {
  if (!HasInput(tok)) {
    SetEof(tok, token);
    return false;
  }

  u8 ch = TakeInput(tok);
  if (ch == expected_ch) {
    tok->state = next_state;
    return true;
  }

  tok->state = kJsonTokenizerStateError;
  SetError(tok, token, "Expected '%c' but got '%c'", expected_ch, ch);
  return false;
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

      case kJsonTokenizerStateStringEscapeU0: {
        OnStringEscapeU(tok, &token, kJsonTokenizerStateStringEscapeU1);
      } break;

      case kJsonTokenizerStateStringEscapeU1: {
        OnStringEscapeU(tok, &token, kJsonTokenizerStateStringEscapeU2);
      } break;

      case kJsonTokenizerStateStringEscapeU2: {
        OnStringEscapeU(tok, &token, kJsonTokenizerStateStringEscapeU3);
      } break;

      case kJsonTokenizerStateStringEscapeU3: {
        OnStringEscapeU(tok, &token, kJsonTokenizerStateString);
      } break;

      case kJsonTokenizerStateStringEnd: {
        OnStringEnd(tok, &token);
      } break;

      case kJsonTokenizerStateInteger: {
        OnInteger(tok, &token);
      } break;

      case kJsonTokenizerStateFraction: {
        OnFraction(tok, &token);
      } break;

      case kJsonTokenizerStateExpoinent: {
        OnExpoinent(tok, &token);
      } break;

      case kJsonTokenizerStateExpoinentNoSign: {
        OnExpoinentNoSign(tok, &token);
      } break;

      case kJsonTokenizerStateNumberEnd: {
        OnNumberEnd(tok, &token);
      } break;

      case kJsonTokenizerStateT: {
        ExpectChar(tok, &token, 'r', kJsonTokenizerStateTr);
      } break;

      case kJsonTokenizerStateTr: {
        ExpectChar(tok, &token, 'u', kJsonTokenizerStateTru);
      } break;

      case kJsonTokenizerStateTru: {
        if (ExpectChar(tok, &token, 'e', kJsonTokenizerStateStart)) {
          token.type = kJsonTokenTrue;
        }
      } break;

      case kJsonTokenizerStateF: {
        ExpectChar(tok, &token, 'a', kJsonTokenizerStateFa);
      } break;

      case kJsonTokenizerStateFa: {
        ExpectChar(tok, &token, 'l', kJsonTokenizerStateFal);
      } break;

      case kJsonTokenizerStateFal: {
        ExpectChar(tok, &token, 's', kJsonTokenizerStateFals);
      } break;

      case kJsonTokenizerStateFals: {
        if (ExpectChar(tok, &token, 'e', kJsonTokenizerStateStart)) {
          token.type = kJsonTokenFalse;
        }
      } break;

      case kJsonTokenizerStateN: {
        ExpectChar(tok, &token, 'u', kJsonTokenizerStateNu);
      } break;

      case kJsonTokenizerStateNu: {
        ExpectChar(tok, &token, 'l', kJsonTokenizerStateNul);
      } break;

      case kJsonTokenizerStateNul: {
        if (ExpectChar(tok, &token, 'l', kJsonTokenizerStateStart)) {
          token.type = kJsonTokenNull;
        }
      } break;

      default:
        UNREACHABLE;
    }
  }
  return token;
}
