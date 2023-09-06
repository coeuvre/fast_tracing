#include "src/json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const usize ERROR_MESSAGE_SIZE = 1024;

enum {
    State_Error,
    State_Start,
    State_Done,
    State_String,
    State_Escape,
    State_EscapeU0,
    State_EscapeU1,
    State_EscapeU2,
    State_EscapeU3,
    State_StringEnd,
    State_Integer,
    State_Fraction,
    State_Expoinent,
    State_ExpoinentNoSign,
    State_NumberEnd,
    State_T,
    State_Tr,
    State_Tru,
    State_F,
    State_Fa,
    State_Fal,
    State_Fals,
    State_N,
    State_Nu,
    State_Nul,
};

JsonTokenizer json_init_tok() {
    JsonTokenizer tok = {};
    tok.arena = memory_arena_init();
    tok.state = State_Start;
    return tok;
}

void json_deinit_tok(JsonTokenizer *tok) {
    memory_arena_deinit(&tok->arena);
    *tok = {};
}

bool json_is_scanning(JsonTokenizer *tok) {
    return tok->state != State_Error && tok->state != State_Done;
}

void json_set_input(JsonTokenizer *tok, Buf input, bool last_input) {
    ASSERT(tok->cursor == tok->input.size &&
           "Last input was not fully consumed");
    tok->input = input;
    tok->last_input = last_input;
    tok->cursor = 0;
}

static void set_error(JsonTokenizer *tok, JsonToken *token, const char *fmt,
                      ...) {
    memory_arena_clear(&tok->arena);

    usize buf_size = ERROR_MESSAGE_SIZE;
    char *buf = (char *)memory_arena_push(&tok->arena, buf_size);

    va_list va;
    va_start(va, fmt);
    usize nwritten = vsnprintf(buf, buf_size, fmt, va);
    va_end(va);

    tok->state = State_Error;
    *token = {
        .type = JsonToken_Error,
        .value = {.data = buf, .size = min(buf_size, nwritten)},
    };
}

static inline bool has_input(JsonTokenizer *tok) {
    return tok->cursor < tok->input.size;
}

static inline u8 take_input(JsonTokenizer *tok) {
    ASSERT(has_input(tok));
    return ((u8 *)tok->input.data)[tok->cursor++];
}

static inline void return_input(JsonTokenizer *tok) {
    ASSERT(tok->cursor > 0);
    tok->cursor--;
}

static void skip_whitespace(JsonTokenizer *tok) {
    while (tok->cursor < tok->input.size) {
        u8 c = ((u8 *)tok->input.data)[tok->cursor];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        tok->cursor++;
    }
}

const usize BUF_SIZE = 1024;

static inline void save_char(JsonTokenizer *tok, u8 ch) {
    ASSERT(tok->buf_cursor <= tok->buf.size);

    while ((tok->buf_cursor + 1) >= tok->buf.size) {
        tok->buf.size = max(BUF_SIZE, tok->buf.size * 2);
        tok->buf.data =
            memory_arena_push(&tok->arena, tok->buf.data, tok->buf.size);
    }

    ASSERT((tok->buf_cursor + 1) < tok->buf.size);
    ((u8 *)tok->buf.data)[tok->buf_cursor++] = ch;
    // Be compatible with C strings
    ((u8 *)tok->buf.data)[tok->buf_cursor] = 0;
}

static void set_eof(JsonTokenizer *tok, JsonToken *token) {
    *token = {.type = JsonToken_Eof};
    if (tok->last_input) {
        tok->state = State_Done;
    }
}

static void on_start(JsonTokenizer *tok, JsonToken *token) {
    skip_whitespace(tok);

    if (!has_input(tok)) {
        set_eof(tok, token);
        return;
    }

    char ch = take_input(tok);
    switch (ch) {
        case '"': {
            ASSERT(tok->buf_cursor == 0);
            tok->state = State_String;
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
            tok->state = State_Integer;
            save_char(tok, ch);
        } break;

        case '[': {
            *token = {.type = JsonToken_ArrayStart};
        } break;

        case ']': {
            *token = {.type = JsonToken_ArrayEnd};
        } break;

        case '{': {
            *token = {.type = JsonToken_ObjectStart};
        } break;

        case '}': {
            *token = {.type = JsonToken_ObjectEnd};
        } break;

        case ':': {
            *token = {.type = JsonToken_Colon};
        } break;

        case ',': {
            *token = {.type = JsonToken_Comma};
        } break;

        case 't': {
            tok->state = State_T;
        } break;

        case 'f': {
            tok->state = State_F;
        } break;

        case 'n': {
            tok->state = State_N;
        } break;

        default: {
            tok->state = State_Error;
            set_error(tok, token, "JSON value expected but got '%c'", ch);
        } break;
    }
}

static void set_string_error_or_eof(JsonTokenizer *tok, JsonToken *token) {
    if (tok->last_input) {
        set_error(tok, token,
                  "End of string '\"' expected but reached end of input");
    } else {
        set_eof(tok, token);
    }
}

static void on_string(JsonTokenizer *tok, JsonToken *token) {
    while (token->type == JsonToken_Unknown) {
        if (!has_input(tok)) {
            set_string_error_or_eof(tok, token);
            return;
        }

        u8 ch = take_input(tok);
        switch (ch) {
            case '"': {
                tok->state = State_StringEnd;
                *token = {
                    .type = JsonToken_String,
                    .value = buf_slice(tok->buf, 0, tok->buf_cursor),
                };
            } break;

            case '\\': {
                save_char(tok, ch);
                tok->state = State_Escape;
                return;
            } break;

            default: {
                save_char(tok, ch);
            } break;
        }
    }
}

static void on_string_escape(JsonTokenizer *tok, JsonToken *token) {
    if (!has_input(tok)) {
        set_string_error_or_eof(tok, token);
        return;
    }

    u8 ch = take_input(tok);
    switch (ch) {
        case '"':
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't': {
            save_char(tok, ch);
            tok->state = State_String;
        } break;

        case 'u': {
            save_char(tok, ch);
            tok->state = State_EscapeU0;
        } break;

        default: {
            tok->state = State_Error;
            set_error(tok, token, "Invalid escape character '\\%c'", ch);
        } break;
    }
}

static void on_string_escape_u(JsonTokenizer *tok, JsonToken *token,
                               u8 next_state) {
    if (!has_input(tok)) {
        set_eof(tok, token);
        return;
    }

    u8 ch = take_input(tok);
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
            save_char(tok, ch);
            tok->state = next_state;
        } break;

        default: {
            tok->state = State_Error;
            set_error(tok, token, "Expected hex digit but got '%c'", ch);
        } break;
    }
}

static void on_string_end(JsonTokenizer *tok, JsonToken *token) {
    memory_arena_pop(&tok->arena, tok->buf.data);
    tok->buf.data = 0;
    tok->buf.size = 0;
    tok->buf_cursor = 0;
    tok->state = State_Start;
}

static void set_number_of_eof(JsonTokenizer *tok, JsonToken *token) {
    if (tok->last_input) {
        tok->state = State_NumberEnd;
        *token = {
            .type = JsonToken_Number,
            .value = buf_slice(tok->buf, 0, tok->buf_cursor),
        };
    } else {
        set_eof(tok, token);
    }
}

static void on_integer(JsonTokenizer *tok, JsonToken *token) {
    while (token->type == JsonToken_Unknown) {
        if (!has_input(tok)) {
            set_number_of_eof(tok, token);
            return;
        }

        u8 ch = take_input(tok);
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
                    return_input(tok);
                    *token = {
                        .type = JsonToken_Number,
                        .value = buf_slice(tok->buf, 0, tok->buf_cursor),
                    };
                    tok->state = State_Start;
                } else {
                    save_char(tok, ch);
                }
            } break;

            case '.': {
                save_char(tok, ch);
                tok->state = State_Fraction;
                return;
            } break;

            case 'e':
            case 'E': {
                save_char(tok, ch);
                tok->state = State_Expoinent;
                return;
            } break;

            default: {
                return_input(tok);
                tok->state = State_NumberEnd;
                *token = {
                    .type = JsonToken_Number,
                    .value = buf_slice(tok->buf, 0, tok->buf_cursor),
                };
            } break;
        }
    }
}

static void on_fraction(JsonTokenizer *tok, JsonToken *token) {
    while (token->type == JsonToken_Unknown) {
        if (!has_input(tok)) {
            set_number_of_eof(tok, token);
            return;
        }

        u8 ch = take_input(tok);
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
                save_char(tok, ch);
            } break;

            case 'e':
            case 'E': {
                save_char(tok, ch);
                tok->state = State_Expoinent;
                return;
            } break;

            default: {
                return_input(tok);
                tok->state = State_NumberEnd;
                *token = {
                    .type = JsonToken_Number,
                    .value = buf_slice(tok->buf, 0, tok->buf_cursor),
                };
            } break;
        }
    }
}

static void on_expoinent(JsonTokenizer *tok, JsonToken *token) {
    while (token->type == JsonToken_Unknown) {
        if (!has_input(tok)) {
            set_number_of_eof(tok, token);
            return;
        }

        u8 ch = take_input(tok);
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
                save_char(tok, ch);
            } break;

            case '+':
            case '-': {
                save_char(tok, ch);
                tok->state = State_ExpoinentNoSign;
                return;
            } break;

            default: {
                return_input(tok);
                tok->state = State_NumberEnd;
                *token = {
                    .type = JsonToken_Number,
                    .value = buf_slice(tok->buf, 0, tok->buf_cursor),
                };
            } break;
        }
    }
}

static void on_expoinent_no_sign(JsonTokenizer *tok, JsonToken *token) {
    while (token->type == JsonToken_Unknown) {
        if (!has_input(tok)) {
            set_number_of_eof(tok, token);
            return;
        }

        u8 ch = take_input(tok);
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
                save_char(tok, ch);
            } break;

            default: {
                return_input(tok);
                tok->state = State_NumberEnd;
                *token = {
                    .type = JsonToken_Number,
                    .value = buf_slice(tok->buf, 0, tok->buf_cursor),
                };
            } break;
        }
    }
}

static void on_number_end(JsonTokenizer *tok, JsonToken *token) {
    memory_arena_pop(&tok->arena, tok->buf.data);
    tok->buf.data = 0;
    tok->buf.size = 0;
    tok->buf_cursor = 0;
    tok->state = State_Start;
}

static bool expect_char(JsonTokenizer *tok, JsonToken *token, char expected_ch,
                        u8 next_state) {
    if (!has_input(tok)) {
        set_eof(tok, token);
        return false;
    }

    u8 ch = take_input(tok);
    if (ch == expected_ch) {
        tok->state = next_state;
        return true;
    }

    tok->state = State_Error;
    set_error(tok, token, "Expected '%c' but got '%c'", expected_ch, ch);
    return false;
}

JsonToken json_get_next_token(JsonTokenizer *tok) {
    ASSERT(tok->cursor <= tok->input.size && "No more input to process");
    ASSERT(json_is_scanning(tok));

    JsonToken token = {.type = JsonToken_Unknown};
    while (token.type == JsonToken_Unknown) {
        switch (tok->state) {
            case State_Start: {
                on_start(tok, &token);
            } break;

            case State_String: {
                on_string(tok, &token);
            } break;

            case State_Escape: {
                on_string_escape(tok, &token);
            } break;

            case State_EscapeU0: {
                on_string_escape_u(tok, &token, State_EscapeU1);
            } break;

            case State_EscapeU1: {
                on_string_escape_u(tok, &token, State_EscapeU2);
            } break;

            case State_EscapeU2: {
                on_string_escape_u(tok, &token, State_EscapeU3);
            } break;

            case State_EscapeU3: {
                on_string_escape_u(tok, &token, State_String);
            } break;

            case State_StringEnd: {
                on_string_end(tok, &token);
            } break;

            case State_Integer: {
                on_integer(tok, &token);
            } break;

            case State_Fraction: {
                on_fraction(tok, &token);
            } break;

            case State_Expoinent: {
                on_expoinent(tok, &token);
            } break;

            case State_ExpoinentNoSign: {
                on_expoinent_no_sign(tok, &token);
            } break;

            case State_NumberEnd: {
                on_number_end(tok, &token);
            } break;

            case State_T: {
                expect_char(tok, &token, 'r', State_Tr);
            } break;

            case State_Tr: {
                expect_char(tok, &token, 'u', State_Tru);
            } break;

            case State_Tru: {
                if (expect_char(tok, &token, 'e', State_Start)) {
                    token.type = JsonToken_True;
                }
            } break;

            case State_F: {
                expect_char(tok, &token, 'a', State_Fa);
            } break;

            case State_Fa: {
                expect_char(tok, &token, 'l', State_Fal);
            } break;

            case State_Fal: {
                expect_char(tok, &token, 's', State_Fals);
            } break;

            case State_Fals: {
                if (expect_char(tok, &token, 'e', State_Start)) {
                    token.type = JsonToken_False;
                }
            } break;

            case State_N: {
                expect_char(tok, &token, 'u', State_Nu);
            } break;

            case State_Nu: {
                expect_char(tok, &token, 'l', State_Nul);
            } break;

            case State_Nul: {
                if (expect_char(tok, &token, 'l', State_Start)) {
                    token.type = JsonToken_Null;
                }
            } break;

            default:
                UNREACHABLE;
        }
    }
    return token;
}
