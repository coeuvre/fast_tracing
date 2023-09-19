#include "src/json.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "src/buf.h"

void json_input_init(JsonInput *input, Buf buf) {
    input->buf = buf;
    input->cursor = 0;
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

// Returns false if there is no more input
static inline bool take_input(MemoryArena *arena, JsonInput *input,
                              JsonToken *token, u8 *ch) {
    ASSERT(input->cursor <= input->buf.size);
    if (input->cursor == input->buf.size) {
        *ch = 0;
        return false;
    }

    *ch = ((u8 *)input->buf.data)[input->cursor++];
    return true;
}

static inline void return_input(JsonInput *input) {
    ASSERT(input->cursor > 0);
    input->cursor--;
}

static bool skip_whitespace(MemoryArena *arena, JsonInput *input,
                            JsonToken *token) {
    while (true) {
        u8 c;
        if (!take_input(arena, input, token, &c)) {
            return false;
        }

        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return_input(input);
            return true;
        }
    }
}

static bool expect(MemoryArena *arena, JsonInput *input, JsonToken *token,
                   JsonError *error, Buf expected) {
    usize start = input->cursor;
    for (usize i = 0; i < expected.size; ++i) {
        u8 expected_ch = expected.data[i];
        u8 actual_ch;
        if (!take_input(arena, input, token, &actual_ch)) {
            set_error(arena, token, error,
                      "Expected '%.*s' but reached end "
                      "of input",
                      (int)expected.size, expected.data);
            return false;
        }
        if (actual_ch != expected_ch) {
            set_error(arena, token, error, "Expected '%.*s' but got '%.*s'",
                      (int)expected.size, expected.data, (int)i,
                      input->buf.data + start);
            return false;
        }
    }
    return true;
}

static bool scan_escape_u(MemoryArena *arena, JsonInput *input,
                          JsonToken *token, JsonError *error) {
    for (int i = 0; i < 4; ++i) {
        u8 ch;
        if (!take_input(arena, input, token, &ch)) {
            return set_error(arena, token, error, "Invalid escape unicode");
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
                        JsonError *error) {
    u8 ch;
    if (!take_input(arena, input, token, &ch)) {
        return set_error(arena, token, error, "Invalid escape character '\\'");
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
            return scan_escape_u(arena, input, token, error);
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
    while (true) {
        u8 ch;
        if (!take_input(arena, input, token, &ch)) {
            return set_error(
                arena, token, error,
                "End of string '\"' expected but reached end of input");
        }

        switch (ch) {
            case '"': {
                *token = {
                    .type = JsonToken_String,
                    .value = buf_slice(input->buf, start, input->cursor - 1),
                };
                return true;
            } break;

            case '\\': {
                if (!scan_escape(arena, input, token, error)) {
                    return false;
                }
            } break;

            default: {
            } break;
        }
    }
}

static bool set_number(MemoryArena *arena, JsonInput *input, JsonToken *token,
                       usize start) {
    *token = {
        .type = JsonToken_Number,
        .value = buf_slice(input->buf, start, input->cursor),
    };
    return true;
}

static bool scan_exponent(MemoryArena *arena, JsonInput *input,
                          JsonToken *token, JsonError *error, usize start,
                          bool has_sign, bool has_digit) {
    while (true) {
        u8 ch;
        if (!take_input(arena, input, token, &ch)) {
            if (!has_digit) {
                Buf value = buf_slice(input->buf, start, input->cursor);
                return set_error(arena, token, error,
                                 "Invalid number '%.*s', expecting a digit but "
                                 "reached end of file",
                                 (int)value.size, value.data);
            }

            return set_number(arena, input, token, start);
        }

        switch (ch) {
            case '-':
            case '+': {
                if (has_digit) {
                    return_input(input);
                    return set_number(arena, input, token, start);
                }

                if (has_sign) {
                    return_input(input);
                    Buf value = buf_slice(input->buf, start, input->cursor);
                    return set_error(arena, token, error,
                                     "Invalid number '%.*s', expecting a "
                                     "digit but got '%c'",
                                     (int)value.size, value.data, ch);
                }

                return scan_exponent(arena, input, token, error, start, true,
                                     false);
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
                    return scan_exponent(arena, input, token, error, start,
                                         has_sign, true);
                }
            } break;

            default: {
                return_input(input);

                if (!has_digit) {
                    Buf value = buf_slice(input->buf, start, input->cursor);
                    return set_error(
                        arena, token, error,
                        "Invalid number '%.*s', expecting a digit but got '%c'",
                        (int)value.size, value.data, ch);
                }

                return set_number(arena, input, token, start);
            } break;
        }
    }
}

static bool scan_fraction(MemoryArena *arena, JsonInput *input,
                          JsonToken *token, JsonError *error, usize start,
                          bool has_digit) {
    while (true) {
        u8 ch;
        if (!take_input(arena, input, token, &ch)) {
            if (!has_digit) {
                Buf value = buf_slice(input->buf, start, input->cursor);
                return set_error(arena, token, error,
                                 "Invalid number '%.*s', expecting a digit but "
                                 "reached end of file",
                                 (int)value.size, value.data);
            }

            return set_number(arena, input, token, start);
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
                    return scan_fraction(arena, input, token, error, start,
                                         true);
                }
            } break;

            case 'e':
            case 'E': {
                if (!has_digit) {
                    return_input(input);
                    Buf value = buf_slice(input->buf, start, input->cursor);
                    return set_error(
                        arena, token, error,
                        "Invalid number '%.*s', expecting a digit but got '%c'",
                        (int)value.size, value.data, ch);
                }

                return scan_exponent(arena, input, token, error, start, true,
                                     false);
            } break;

            default: {
                return_input(input);

                if (!has_digit) {
                    Buf value = buf_slice(input->buf, start, input->cursor);
                    return set_error(
                        arena, token, error,
                        "Invalid number '%.*s', expecting a digit but got '%c'",
                        (int)value.size, value.data, ch);
                }

                return set_number(arena, input, token, start);
            } break;
        }
    }
}

static bool scan_integer(MemoryArena *arena, JsonInput *input, JsonToken *token,
                         JsonError *error, usize start) {
    while (true) {
        u8 ch;
        if (!take_input(arena, input, token, &ch)) {
            return set_number(arena, input, token, start);
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
                return scan_fraction(arena, input, token, error, start, false);
            } break;

            case 'e':
            case 'E': {
                return scan_exponent(arena, input, token, error, start, true,
                                     false);
            } break;

            default: {
                return_input(input);
                return set_number(arena, input, token, start);
            } break;
        }
    }
}

static bool scan_number(MemoryArena *arena, JsonInput *input, JsonToken *token,
                        JsonError *error, usize start, bool has_minus) {
    u8 ch;
    if (!take_input(arena, input, token, &ch)) {
        return false;
    }

    switch (ch) {
        case '0': {
            if (!take_input(arena, input, token, &ch)) {
                return false;
            }
            switch (ch) {
                case '.': {
                    return scan_fraction(arena, input, token, error, start,
                                         false);
                } break;

                case 'e':
                case 'E': {
                    return scan_exponent(arena, input, token, error, start,
                                         false, false);
                }
                default: {
                    return_input(input);
                    return set_number(arena, input, token, start);
                } break;
            }
        } break;

        case '-': {
            if (has_minus) {
                return_input(input);
                return set_error(
                    arena, token, error,
                    "Invalid number '-', expecting a digit but got '-'");
            }

            return scan_number(arena, input, token, error, start, true);
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
            return scan_integer(arena, input, token, error, start);
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

    if (!skip_whitespace(arena, input, token)) {
        return false;
    }

    u8 ch;
    if (!take_input(arena, input, token, &ch)) {
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
            return scan_number(arena, input, token, error, input->cursor,
                               false);
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

    return_input(input);
    Buf remaining = buf_slice(input->buf, input->cursor, input->buf.size);
    printf("cursor: %d, input: %.*s\n", (int)input->cursor, (int)remaining.size,
           remaining.data);

    return set_error(arena, token, error, "JSON value expected but got '%c'",
                     ch);
}
