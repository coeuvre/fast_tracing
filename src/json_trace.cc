#include "src/json_trace.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>

enum {
    // Initial state. we need to skip whitespace and find a '{' or '[',
    // otherwise it's an error.
    State_Init,
    // We got { as first non-whitespace character, or we have processed one
    // key-value pair in this object (and skipped ',' if any). We now need to
    // look at next object key and update state accordingly.
    State_ObjectFormat,
    // We were looking at object key in State_ObjectFormat and we got a starting
    // ", but we reached end of input before we get the ending ". We need more
    // input from the caller and the previous input is saved in the internal
    // buffer.
    State_ObjectFormat_Continued,
    // We got "traceEvents" key. We need to eat ARRAY_BEGIN and update the state
    // to State_ArrayFormat.
    State_ObjectFormat_TraceEvents,
    // We got keys that are not supported (yet). We need skip the
    // value, and then return to State_ObjectFormat.
    State_ObjectFormat_UnknownKey,
    // We got [ as the first non-whitespace character, or we got "traceEvents"
    // key in the object format, or we have processed one array item. We now
    // need to process next array item.
    State_ArrayFormat,
    // We need to skip whitespace characters until we find a target. Skip it and
    // update to the next state.
    State_SkipChar,

    State_Error,
    State_Done,
};

static const usize INITIAL_BUF_SIZE = 4096;

static void ensure_buf_size(JsonTraceParser *parser, usize size) {
    usize new_size = max(INITIAL_BUF_SIZE, parser->buf.size);
    while (new_size < size) {
        new_size <<= 1;
    }
    if (new_size > parser->buf.size) {
        parser->buf.size = new_size;
        parser->buf.data = (u8 *)memory_arena_realloc(
            parser->arena, parser->buf.data, new_size);
        ASSERT(parser->buf.data);
    }
}

static JsonTraceResult set_error(JsonTraceParser *parser, const char *fmt,
                                 ...) {
    ASSERT(parser->state != State_Error);

    ensure_buf_size(parser, INITIAL_BUF_SIZE);

    va_list va;
    va_start(va, fmt);
    vsnprintf((char *)parser->buf.data, parser->buf.size, fmt, va);
    va_end(va);

    parser->state = State_Error;

    return JsonTraceResult_Error;
}

// Returns true if cursor points to a non-whitespace character
static bool skip_whitespace(Buf buf, usize *cursor) {
    while (*cursor < buf.size) {
        u8 ch = buf.data[*cursor];
        switch (ch) {
            case ' ':
            case '\t':
            case '\n':
            case '\r': {
                (*cursor)++;
            } break;

            default: {
                return true;
            } break;
        }
    }
    return false;
}

void json_trace_parser_init(JsonTraceParser *parser, MemoryArena *arena) {
    *parser = {
        .arena = arena,
        .state = State_Init,
    };
}

void json_trace_parser_deinit(JsonTraceParser *parser) {
    parser->state = State_Done;
}

static void save_input(JsonTraceParser *parser, usize *cursor, Buf input) {
    if (input.size == 0) {
        return;
    }

    if (*cursor + input.size > parser->buf.size) {
        parser->buf.size = max(INITIAL_BUF_SIZE, *cursor + input.size);
        parser->buf.data = (u8 *)memory_arena_realloc(
            parser->arena, parser->buf.data, parser->buf.size);
        ASSERT(parser->buf.data);
    }
    memcpy(parser->buf.data + *cursor, input.data, input.size);
    *cursor += input.size;
}

static void handle_object_format_key(JsonTraceParser *parser, Buf key) {
    // if (buf_equal(key, STR_LITERAL("traceEvents"))) {
    //     parser->state = State_ObjectFormat_TraceEvents;
    // } else {
    parser->state = State_SkipChar;
    parser->skip_char.target = ':';
    parser->skip_char.next_state = State_ObjectFormat_UnknownKey;
    // }
}

static void push_stack(JsonTraceParser *parser, u8 ch) {
    ensure_buf_size(parser, parser->unknown_key.stack_cursor + 1);
    parser->buf.data[parser->unknown_key.stack_cursor++] = ch;
}

static void pop_stack(JsonTraceParser *parser) {
    ASSERT(parser->unknown_key.stack_cursor > 0);
    parser->unknown_key.stack_cursor--;
}

static bool is_stack_top(JsonTraceParser *parser, char ch) {
    if (parser->unknown_key.stack_cursor == 0) {
        return false;
    }
    return parser->buf.data[parser->unknown_key.stack_cursor - 1] == ch;
}

static bool is_stack_empty(JsonTraceParser *parser) {
    return parser->unknown_key.stack_cursor == 0;
}

JsonTraceResult json_trace_parser_parse(JsonTraceParser *parser, Trace *trace,
                                        Buf buf) {
    usize cursor = 0;
    while (true) {
        switch (parser->state) {
            case State_Init: {
                if (!skip_whitespace(buf, &cursor)) {
                    return JsonTraceResult_NeedMoreInput;
                }
                u8 ch = buf.data[cursor];
                switch (ch) {
                    case '{': {
                        parser->state = State_ObjectFormat;
                        cursor += 1;
                    } break;

                    case '[': {
                        parser->state = State_ArrayFormat;
                        cursor += 1;
                    } break;

                    default: {
                        return set_error(
                            parser,
                            "Invalid JSON Trace: expected '{' or '[' but got "
                            "'%c'",
                            ch);
                    } break;
                }
            } break;

            case State_ObjectFormat: {
                if (!skip_whitespace(buf, &cursor)) {
                    return JsonTraceResult_NeedMoreInput;
                }

                u8 ch = buf.data[cursor];
                switch (ch) {
                    case '"': {
                        cursor += 1;
                        usize start = cursor;

                        ASSERT(cursor <= buf.size);
                        while (true) {
                            if (cursor == buf.size) {
                                parser->object_format.buf_cursor = 0;
                                save_input(parser,
                                           &parser->object_format.buf_cursor,
                                           buf_slice(buf, start, cursor));
                                return JsonTraceResult_NeedMoreInput;
                            }

                            ch = buf.data[cursor];
                            cursor += 1;
                            if (ch == '"' && buf.data[cursor - 2] != '\\') {
                                Buf key = buf_slice(buf, start, cursor - 1);
                                handle_object_format_key(parser, key);
                                break;
                            }
                        }
                    } break;

                    case '}': {
                        parser->state = State_Done;
                        return JsonTraceResult_Done;
                    } break;

                    default: {
                        return set_error(
                            parser,
                            "Invalid JSON Trace: expected '\"' but got "
                            "'%c'",
                            ch);
                    } break;
                }
            } break;

            case State_ObjectFormat_Continued: {
                ASSERT(cursor == 0);
                bool found_key = false;
                while (cursor < buf.size) {
                    u8 ch = buf.data[cursor];
                    if (ch == '"') {
                        u8 last_char;
                        if (cursor == 0) {
                            last_char =
                                parser->buf
                                    .data[parser->object_format.buf_cursor - 1];
                        } else {
                            last_char = buf.data[cursor - 1];
                        }

                        if (last_char != '\\') {
                            save_input(parser,
                                       &parser->object_format.buf_cursor,
                                       buf_slice(buf, 0, cursor));
                            Buf key =
                                buf_slice(parser->buf, 0,
                                          parser->object_format.buf_cursor);
                            handle_object_format_key(parser, key);
                            found_key = true;
                            break;
                        }
                    }
                }
                if (!found_key) {
                    save_input(parser, &parser->object_format.buf_cursor, buf);
                    return JsonTraceResult_NeedMoreInput;
                }
            } break;

            case State_ObjectFormat_UnknownKey: {
                // skip until ',' or '}' that are not inside a string, array or
                // object.

                // Always skip whitespace even inside a string since we don't
                // care about the actual string value and whitespace won't
                // change the result.
                if (!skip_whitespace(buf, &cursor)) {
                    return JsonTraceResult_NeedMoreInput;
                }

                if (cursor > 0) {
                    parser->unknown_key.last_char = buf.data[cursor - 1];
                }
                while (cursor < buf.size) {
                    u8 ch = buf.data[cursor++];
                    switch (ch) {
                        case '[':
                        case '{': {
                            if (!is_stack_top(parser, '"')) {
                                push_stack(parser, ch);
                            }
                        } break;

                        case '"': {
                            if (is_stack_top(parser, '"')) {
                                if (parser->unknown_key.last_char != '\\') {
                                    pop_stack(parser);
                                }
                            } else {
                                push_stack(parser, '"');
                            }
                        } break;

                        case ']': {
                            if (is_stack_top(parser, '[')) {
                                pop_stack(parser);
                            } else if (is_stack_top(parser, '"')) {
                            } else {
                                return set_error(
                                    parser,
                                    "Invalid JSON Trace: unexpected ']'");
                            }
                        } break;

                        case '}': {
                            if (is_stack_top(parser, '{')) {
                                pop_stack(parser);
                            } else if (is_stack_top(parser, '"')) {
                            } else if (is_stack_empty(parser)) {
                                parser->state = State_ObjectFormat;
                                break;
                            } else {
                                u8 top =
                                    parser->buf
                                        .data[parser->unknown_key.stack_cursor -
                                              1];
                                return set_error(
                                    parser,
                                    "Invalid JSON Trace: unexpected '}', stack "
                                    "top: %c",
                                    top);
                            }
                        } break;

                        case ',': {
                            if (is_stack_empty(parser)) {
                                parser->state = State_ObjectFormat;
                                break;
                            }
                        } break;
                    }
                    parser->unknown_key.last_char = ch;
                }
            } break;

            case State_SkipChar: {
                if (!skip_whitespace(buf, &cursor)) {
                    return JsonTraceResult_NeedMoreInput;
                }
                u8 ch = buf.data[cursor];
                if (ch != parser->skip_char.target) {
                    return set_error(
                        parser,
                        "Invalid JSON Trace: expected '%c' but got '%c'",
                        parser->skip_char.target, ch);
                }
                cursor += 1;
                parser->state = parser->skip_char.next_state;
                switch (parser->state) {
                    case State_ObjectFormat_UnknownKey: {
                        parser->unknown_key = {};
                    } break;

                    default: {
                        UNREACHABLE;
                    } break;
                }
            } break;

            default: {
                UNREACHABLE;
            } break;
        }
    }
}

char *json_trace_parser_get_error(JsonTraceParser *parser) {
    ASSERT(parser->state == State_Error);
    return (char *)parser->buf.data;
}
