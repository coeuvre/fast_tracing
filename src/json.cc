#include "src/json.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void json_input_init(JsonInput *input, void *ctx, JsonInput_Fetch *fetch) {
    *input = {
        .ctx = ctx,
        .fetch = fetch,
    };
}

static const usize ERROR_MESSAGE_SIZE = 1024;

static bool set_error(MemoryArena *arena, JsonToken *token, JsonError *error,
                      const char *fmt, ...) {
    usize buf_size = ERROR_MESSAGE_SIZE;
    char *buf = (char *)memory_arena_alloc(arena, buf_size);

    va_list va;
    va_start(va, fmt);
    usize nwritten = vsnprintf(buf, buf_size, fmt, va);
    va_end(va);

    *token = {.type = JsonToken_Eof};
    *error = {.has_error = true,
              .message = {.data = (u8 *)buf, .size = nwritten}};

    return false;
}

// Returns false if there is no more input or an error occurred
static inline bool take_input(MemoryArena *arena, JsonInput *input,
                              JsonToken *token, JsonError *error, u8 *ch) {
    if (input->cursor < input->buf.size) {
        *ch = ((u8 *)input->buf.data)[input->cursor++];
        return true;
    }

    ASSERT(input->cursor == input->buf.size);

    while (true) {
        input->cursor = 0;
        if (!(input->fetch(input->ctx, arena, &input->buf, error))) {
            *ch = 0;
            return false;
        }
        if (input->buf.size > 0) {
            break;
        }
    }

    if (input->buf.size == 0) {
        *ch = 0;
        return false;
    }

    *ch = input->buf.data[input->cursor++];
    return true;
}

static inline void return_input(JsonInput *input) {
    ASSERT(input->cursor > 0);
    input->cursor--;
}

static bool skip_whitespace(MemoryArena *arena, JsonInput *input,
                            JsonToken *token, JsonError *error) {
    while (true) {
        u8 c;
        if (!take_input(arena, input, token, error, &c)) {
            return false;
        }

        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return_input(input);
            return true;
        }
    }
}

const usize BUF_SIZE = 1024;

static bool expect(MemoryArena *arena, JsonInput *input, JsonToken *token,
                   JsonError *error, Buf expected) {
    for (usize i = 0; i < expected.size; ++i) {
        u8 expected_ch = expected.data[i];
        u8 actual_ch;
        if (!take_input(arena, input, token, error, &actual_ch)) {
            return false;
        }
        if (actual_ch != expected_ch) {
            return false;
        }
    }
    return true;
}

static void save_buf(MemoryArena *arena, Buf *buf, usize *buf_cursor,
                     Buf data) {
    while (*buf_cursor + data.size >= buf->size) {
        if (buf->size == 0) {
            buf->size = BUF_SIZE;
        } else {
            buf->size <<= 1;
        }
        buf->data = (u8 *)memory_arena_realloc(arena, buf->data, buf->size);
        ASSERT(buf->data);
    }

    memcpy(buf->data + *buf_cursor, data.data, data.size);
    *buf_cursor += data.size;
}

static bool take_and_save_input(MemoryArena *arena, JsonInput *input,
                                JsonToken *token, JsonError *error, u8 *ch,
                                Buf *buf, usize *buf_cursor, usize *start) {
    if (input->cursor == input->buf.size) {
        save_buf(arena, buf, buf_cursor,
                 buf_slice(input->buf, *start, input->cursor));
        *start = 0;
    }

    if (!take_input(arena, input, token, error, ch)) {
        return false;
    }

    return true;
}

static void return_saved_input(JsonInput *input, usize *buf_cursor) {
    return_input(input);
    if (*buf_cursor > 0) {
        *buf_cursor -= 1;
    }
}

static bool scan_escape_u(MemoryArena *arena, JsonInput *input,
                          JsonToken *token, JsonError *error, Buf *buf,
                          usize *buf_cursor, usize *start) {
    for (int i = 0; i < 4; ++i) {
        u8 ch;
        if (!take_and_save_input(arena, input, token, error, &ch, buf,
                                 buf_cursor, start)) {
            if (!error->has_error) {
                return set_error(arena, token, error, "Invalid escape unicode");
            }
            return false;
        }

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
            } break;

            default: {
                return set_error(arena, token, error,
                                 "Expected hex digit but got '%c'", ch);
            } break;
        }
    }

    return true;
}

static bool scan_escape(MemoryArena *arena, JsonInput *input, JsonToken *token,
                        JsonError *error, Buf *buf, usize *buf_cursor,
                        usize *start) {
    u8 ch;
    if (!take_and_save_input(arena, input, token, error, &ch, buf, buf_cursor,
                             start)) {
        if (!error->has_error) {
            return set_error(arena, token, error,
                             "Invalid escape character '\\'");
        }
        return false;
    }

    switch (ch) {
        case '"':
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't': {
            return true;
        } break;

        case 'u': {
            return scan_escape_u(arena, input, token, error, buf, buf_cursor,
                                 start);
        } break;

        default: {
            return set_error(arena, token, error,
                             "Invalid escape character '\\%c'", ch);
        } break;
    }
}

static bool scan_string(MemoryArena *arena, JsonInput *input, JsonToken *token,
                        JsonError *error) {
    usize start = input->cursor;
    Buf *buf = &input->backing_buf;
    usize *buf_cursor = &input->backing_buf_cursor;
    *buf_cursor = 0;

    while (true) {
        u8 ch;
        if (!take_and_save_input(arena, input, token, error, &ch, buf,
                                 buf_cursor, &start)) {
            if (!error->has_error) {
                return set_error(
                    arena, token, error,
                    "End of string '\"' expected but reached end of input");
            }
            return false;
        }

        switch (ch) {
            case '"': {
                if (*buf_cursor > 0) {
                    save_buf(arena, buf, buf_cursor,
                             buf_slice(input->buf, start, input->cursor - 1));
                    *token = {
                        .type = JsonToken_String,
                        .value = buf_slice(*buf, 0, *buf_cursor),
                    };
                } else {
                    *token = {
                        .type = JsonToken_String,
                        .value =
                            buf_slice(input->buf, start, input->cursor - 1),
                    };
                }
                return true;
            } break;

            case '\\': {
                if (!scan_escape(arena, input, token, error, buf, buf_cursor,
                                 &start)) {
                    return false;
                }
            } break;

            default: {
            } break;
        }
    }
}

static Buf end_save_input(MemoryArena *arena, JsonInput *input, Buf *buf,
                          usize *buf_cursor, usize start) {
    if (*buf_cursor > 0) {
        save_buf(arena, buf, buf_cursor,
                 buf_slice(input->buf, start, input->cursor));
        return buf_slice(*buf, 0, *buf_cursor);
    } else {
        return buf_slice(input->buf, start, input->cursor);
    }
}

static bool set_number(MemoryArena *arena, JsonInput *input, JsonToken *token,
                       Buf *buf, usize *buf_cursor, usize start) {
    *token = {
        .type = JsonToken_Number,
        .value = end_save_input(arena, input, buf, buf_cursor, start),
    };
    return true;
}

static bool scan_exponent(MemoryArena *arena, JsonInput *input,
                          JsonToken *token, JsonError *error, Buf *buf,
                          usize *buf_cursor, usize *start, bool has_sign,
                          bool has_digit) {
    while (true) {
        u8 ch;
        if (!take_and_save_input(arena, input, token, error, &ch, buf,
                                 buf_cursor, start)) {
            return false;
        }
        switch (ch) {
            case '-':
            case '+': {
                if (has_digit) {
                    return_saved_input(input, buf_cursor);
                    return set_number(arena, input, token, buf, buf_cursor,
                                      *start);
                }

                if (has_sign) {
                    return_saved_input(input, buf_cursor);
                    Buf value =
                        end_save_input(arena, input, buf, buf_cursor, *start);
                    return set_error(arena, token, error,
                                     "Invalid number '%.*s', expecting a "
                                     "digit but got '%c'",
                                     (int)value.size, value.data, ch);
                }

                return scan_exponent(arena, input, token, error, buf,
                                     buf_cursor, start, true, false);
            } break;

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
                if (!has_digit) {
                    return scan_exponent(arena, input, token, error, buf,
                                         buf_cursor, start, has_sign, true);
                }
            } break;

            default: {
                return_saved_input(input, buf_cursor);

                if (!has_digit) {
                    Buf value =
                        end_save_input(arena, input, buf, buf_cursor, *start);
                    return set_error(
                        arena, token, error,
                        "Invalid number '%.*s', expecting a digit but got '%c'",
                        (int)value.size, value.data, ch);
                }

                return set_number(arena, input, token, buf, buf_cursor, *start);
            } break;
        }
    }
}

static bool scan_fraction(MemoryArena *arena, JsonInput *input,
                          JsonToken *token, JsonError *error, Buf *buf,
                          usize *buf_cursor, usize *start, bool has_digit) {
    while (true) {
        u8 ch;
        if (!take_and_save_input(arena, input, token, error, &ch, buf,
                                 buf_cursor, start)) {
            return false;
        }

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
                if (!has_digit) {
                    return scan_fraction(arena, input, token, error, buf,
                                         buf_cursor, start, true);
                }
            } break;

            case 'e':
            case 'E': {
                if (!has_digit) {
                    return_saved_input(input, buf_cursor);
                    Buf value =
                        end_save_input(arena, input, buf, buf_cursor, *start);
                    return set_error(
                        arena, token, error,
                        "Invalid number '%.*s', expecting a digit but got '%c'",
                        (int)value.size, value.data, ch);
                }

                return scan_exponent(arena, input, token, error, buf,
                                     buf_cursor, start, true, false);
            } break;

            default: {
                return_saved_input(input, buf_cursor);

                if (!has_digit) {
                    Buf value =
                        end_save_input(arena, input, buf, buf_cursor, *start);
                    return set_error(
                        arena, token, error,
                        "Invalid number '%.*s', expecting a digit but got '%c'",
                        (int)value.size, value.data, ch);
                }

                return set_number(arena, input, token, buf, buf_cursor, *start);
            } break;
        }
    }
}

static bool scan_integer(MemoryArena *arena, JsonInput *input, JsonToken *token,
                         JsonError *error, Buf *buf, usize *buf_cursor,
                         usize *start) {
    while (true) {
        u8 ch;
        if (!take_and_save_input(arena, input, token, error, &ch, buf,
                                 buf_cursor, start)) {
            return false;
        }

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
            } break;

            case '.': {
                return scan_fraction(arena, input, token, error, buf,
                                     buf_cursor, start, false);
            } break;

            case 'e':
            case 'E': {
                return scan_exponent(arena, input, token, error, buf,
                                     buf_cursor, start, true, false);
            } break;

            default: {
                return_saved_input(input, buf_cursor);
                return set_number(arena, input, token, buf, buf_cursor, *start);
            } break;
        }
    }
}

static bool scan_number(MemoryArena *arena, JsonInput *input, JsonToken *token,
                        JsonError *error, Buf *buf, usize *buf_cursor,
                        usize *start, bool has_minus) {
    u8 ch;
    if (!take_and_save_input(arena, input, token, error, &ch, buf, buf_cursor,
                             start)) {
        return false;
    }

    switch (ch) {
        case '0': {
            if (!take_and_save_input(arena, input, token, error, &ch, buf,
                                     buf_cursor, start)) {
                return false;
            }
            switch (ch) {
                case '.': {
                    return scan_fraction(arena, input, token, error, buf,
                                         buf_cursor, start, false);
                } break;

                case 'e':
                case 'E': {
                    return scan_exponent(arena, input, token, error, buf,
                                         buf_cursor, start, false, false);
                }
                default: {
                    return_saved_input(input, buf_cursor);
                    return set_number(arena, input, token, buf, buf_cursor,
                                      *start);
                } break;
            }
        } break;

        case '-': {
            if (has_minus) {
                return_saved_input(input, buf_cursor);
                return set_error(
                    arena, token, error,
                    "Invalid number '-', expecting a digit but got '-'");
            }

            return scan_number(arena, input, token, error, buf, buf_cursor,
                               start, true);
        } break;

        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            return scan_integer(arena, input, token, error, buf, buf_cursor,
                                start);
        } break;

        default: {
            UNREACHABLE;
            return false;
        } break;
    }
}

bool json_scan(MemoryArena *arena, JsonInput *input, JsonToken *token,
               JsonError *error) {
    *token = {};
    *error = {};

    if (!skip_whitespace(arena, input, token, error)) {
        return false;
    }

    u8 ch;
    if (!take_input(arena, input, token, error, &ch)) {
        return false;
    }

    switch (ch) {
        case '"': {
            return scan_string(arena, input, token, error);
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
            return_input(input);

            usize start = input->cursor;
            Buf *buf = &input->backing_buf;
            usize *buf_cursor = &input->backing_buf_cursor;
            *buf_cursor = 0;
            return scan_number(arena, input, token, error, buf, buf_cursor,
                               &start, false);
        } break;

        case '[': {
            *token = {.type = JsonToken_ArrayStart};
            return true;
        } break;

        case ']': {
            *token = {.type = JsonToken_ArrayEnd};
            return true;
        } break;

        case '{': {
            *token = {.type = JsonToken_ObjectStart};
            return true;
        } break;

        case '}': {
            *token = {.type = JsonToken_ObjectEnd};
            return true;
        } break;

        case ':': {
            *token = {.type = JsonToken_Colon};
            return true;
        } break;

        case ',': {
            *token = {.type = JsonToken_Comma};
            return true;
        } break;

        case 't': {
            if (expect(arena, input, token, error, STR_LITERAL("rue"))) {
                *token = {.type = JsonToken_True};
                return true;
            }
        } break;

        case 'f': {
            if (expect(arena, input, token, error, STR_LITERAL("alse"))) {
                *token = {.type = JsonToken_False};
                return true;
            }
        } break;

        case 'n': {
            if (expect(arena, input, token, error, STR_LITERAL("ull"))) {
                *token = {.type = JsonToken_Null};
                return true;
            }
        } break;

        default: {
        } break;
    }

    return set_error(arena, token, error, "JSON value expected but got '%c'",
                     ch);
}
