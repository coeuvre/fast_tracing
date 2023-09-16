#include "src/json_trace.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>

#include "src/buf.h"

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
    State_ObjectFormat_Key_Continued,
    // We got "traceEvents" key. We need to eat ARRAY_BEGIN and update the state
    // to State_ArrayFormat.
    State_ObjectFormat_TraceEvents,
    // We got keys that are not supported (yet). We need skip the
    // value, and then return to State_ObjectFormat, or State_Done if no more
    // keys.
    State_ObjectFormat_UnknownKey,
    // We have processed a key-value pair. We need to eat a COMMA or OBJECT_END.
    State_ObjectFormat_AfterValue,
    // We got [ as the first non-whitespace character, or we got "traceEvents"
    // key in the object format, or we have processed one array item. We now
    // need to process next array item.
    State_ArrayFormat,
    // We have consumed a TraceEvent, we now need to consume either a comma and
    // jump to State_ArrayFormat, or we need to consume an ARRAY_END and jump to
    // parent state (either done or State_ObjectFormat).
    State_ArrayFormat_AfterTraceEvent,
    // The cursor was inside a TraceEvent, but we reached end of input. We need
    // consume more input to reach the end of the TraceEvent.
    State_ArrayFormat_TraceEvent_Continued,
    // We need to skip whitespace characters until we find a target. Skip it and
    // update to the next state.
    State_SkipChar,

    State_Error,
    State_Done,
};

static const usize INITIAL_BUF_SIZE = 4096;

static void ensure_buf_size(MemoryArena *arena, Buf *buf, usize size) {
    usize new_size = max(INITIAL_BUF_SIZE, buf->size);
    while (new_size < size) {
        new_size <<= 1;
    }
    if (new_size > buf->size) {
        buf->size = new_size;
        buf->data = (u8 *)memory_arena_realloc(arena, buf->data, new_size);
        ASSERT(buf->data);
    }
}

static JsonTraceResult set_error(JsonTraceParser *parser, const char *fmt,
                                 ...) {
    ASSERT(parser->state != State_Error);

    ensure_buf_size(parser->arena, &parser->buf, INITIAL_BUF_SIZE);

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
    parser->state = State_SkipChar;
    parser->skip_char.target = ':';
    if (buf_equal(key, STR_LITERAL("traceEvents"))) {
        parser->skip_char.next_state = State_ObjectFormat_TraceEvents;
    } else {
        parser->skip_char.next_state = State_ObjectFormat_UnknownKey;
    }
}

static void push_stack(JsonTraceParser *parser, u8 ch) {
    ensure_buf_size(parser->arena, &parser->stack, parser->stack_cursor + 1);
    parser->stack.data[parser->stack_cursor++] = ch;
}

static void pop_stack(JsonTraceParser *parser) {
    ASSERT(parser->stack_cursor > 0);
    parser->stack_cursor--;
}

static bool is_stack_top(JsonTraceParser *parser, char ch) {
    if (parser->stack_cursor == 0) {
        return false;
    }
    return parser->stack.data[parser->stack_cursor - 1] == ch;
}

static bool is_stack_empty(JsonTraceParser *parser) {
    return parser->stack_cursor == 0;
}

static void handle_trace_event(Buf trace_event) {
    // printf("%.*s\n", (int)trace_event.size, trace_event.data);
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
                        parser->has_object_format = true;
                        cursor += 1;
                    } break;

                    case '[': {
                        parser->state = State_ArrayFormat;
                        parser->buf_cursor = 0;
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
                                parser->buf_cursor = 0;
                                save_input(parser, &parser->buf_cursor,
                                           buf_slice(buf, start, cursor));
                                parser->state =
                                    State_ObjectFormat_Key_Continued;
                                return JsonTraceResult_NeedMoreInput;
                            }

                            ch = buf.data[cursor++];
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

            case State_ObjectFormat_Key_Continued: {
                ASSERT(cursor == 0);
                bool found_key = false;
                while (cursor < buf.size) {
                    u8 ch = buf.data[cursor];
                    if (ch == '"') {
                        u8 last_char;
                        if (cursor == 0) {
                            last_char =
                                parser->buf.data[parser->buf_cursor - 1];
                        } else {
                            last_char = buf.data[cursor - 1];
                        }

                        if (last_char != '\\') {
                            save_input(parser, &parser->buf_cursor,
                                       buf_slice(buf, 0, cursor));
                            Buf key =
                                buf_slice(parser->buf, 0, parser->buf_cursor);
                            handle_object_format_key(parser, key);
                            found_key = true;
                            break;
                        }
                    }
                }
                if (!found_key) {
                    save_input(parser, &parser->buf_cursor, buf);
                    return JsonTraceResult_NeedMoreInput;
                }
            } break;

            case State_ObjectFormat_TraceEvents: {
                if (!skip_whitespace(buf, &cursor)) {
                    return JsonTraceResult_NeedMoreInput;
                }

                u8 ch = buf.data[cursor];
                if (ch != '[') {
                    return set_error(
                        parser, "Invalid JSON Trace: expected '[' but got '%c'",
                        ch);
                }

                cursor += 1;

                parser->state = State_ArrayFormat;
                parser->buf_cursor = 0;
            } break;

            case State_ObjectFormat_UnknownKey: {
                // skip until ',' or '}' that are not inside a string, array or
                // object.

                if (!parser->unknown_key.init) {
                    if (!skip_whitespace(buf, &cursor)) {
                        return JsonTraceResult_NeedMoreInput;
                    }

                    parser->stack_cursor = 0;
                    u8 ch = buf.data[cursor++];
                    switch (ch) {
                        case '"':
                        case '{':
                        case '[': {
                            push_stack(parser, ch);
                        } break;
                        default: {
                        } break;
                    }
                    parser->unknown_key.last_char = ch;
                    parser->unknown_key.init = true;
                }

                if (parser->stack_cursor > 0) {
                    bool done = false;
                    switch (parser->stack.data[0]) {
                        case '"': {
                            while (cursor < buf.size && !done) {
                                u8 ch = buf.data[cursor++];
                                if (ch == '"' &&
                                    parser->unknown_key.last_char != '\\') {
                                    pop_stack(parser);
                                    ASSERT(is_stack_empty(parser));
                                    parser->state =
                                        State_ObjectFormat_AfterValue;
                                    done = true;
                                }
                                parser->unknown_key.last_char = ch;
                            }
                        } break;

                        case '{': {
                            while (cursor < buf.size && !done) {
                                u8 ch = buf.data[cursor++];
                                switch (ch) {
                                    case '"': {
                                        if (parser->unknown_key.last_char !=
                                            '\\') {
                                            if (is_stack_top(parser, '"')) {
                                                pop_stack(parser);
                                            } else {
                                                push_stack(parser, '"');
                                            }
                                        }
                                    } break;

                                    case '{': {
                                        if (!is_stack_top(parser, '"')) {
                                            push_stack(parser, '{');
                                        }
                                    } break;

                                    case '}': {
                                        if (!is_stack_top(parser, '"')) {
                                            if (is_stack_empty(parser)) {
                                                parser->state =
                                                    State_ObjectFormat_AfterValue;
                                                done = true;
                                            } else {
                                                pop_stack(parser);
                                            }
                                        }
                                    } break;
                                }

                                parser->unknown_key.last_char = ch;
                            }
                        } break;

                        case '[': {
                            while (cursor < buf.size && !done) {
                                u8 ch = buf.data[cursor++];
                                switch (ch) {
                                    case '"': {
                                        if (parser->unknown_key.last_char !=
                                            '\\') {
                                            if (is_stack_top(parser, '"')) {
                                                pop_stack(parser);
                                            } else {
                                                push_stack(parser, '"');
                                            }
                                        }
                                    } break;

                                    case '[': {
                                        if (!is_stack_top(parser, '"')) {
                                            push_stack(parser, '[');
                                        }
                                    } break;

                                    case ']': {
                                        if (!is_stack_top(parser, '"')) {
                                            if (is_stack_empty(parser)) {
                                                parser->state =
                                                    State_ObjectFormat_AfterValue;
                                                done = true;
                                            } else {
                                                pop_stack(parser);
                                            }
                                        }
                                    } break;
                                }

                                parser->unknown_key.last_char = ch;
                            }
                        } break;

                        default: {
                            UNREACHABLE;
                        } break;
                    }
                    if (!done) {
                        return JsonTraceResult_NeedMoreInput;
                    }
                } else {
                    bool found = false;
                    bool done = false;
                    while (cursor < buf.size) {
                        u8 ch = buf.data[cursor++];
                        if (ch == ',') {
                            found = true;
                            break;
                        } else if (ch == '}') {
                            found = true;
                            done = true;
                            break;
                        }
                    }
                    if (found) {
                        if (done) {
                            parser->state = State_Done;
                            return JsonTraceResult_Done;
                        } else {
                            parser->state = State_ObjectFormat;
                        }
                    }
                }
            } break;

            case State_ObjectFormat_AfterValue: {
                if (!skip_whitespace(buf, &cursor)) {
                    return JsonTraceResult_NeedMoreInput;
                }

                u8 ch = buf.data[cursor];
                switch (ch) {
                    case ',': {
                        cursor += 1;
                        parser->state = State_ObjectFormat;
                    } break;

                    case '}': {
                        cursor += 1;
                        parser->state = State_Done;
                        return JsonTraceResult_Done;
                    } break;

                    default: {
                        return set_error(
                            parser,
                            "Invalid JSON Trace: expected ',' or '}' but got "
                            "'%c'",
                            ch);
                    } break;
                }
            } break;

            case State_ArrayFormat: {
                usize start = cursor;

                if (parser->buf_cursor == 0) {
                    if (!skip_whitespace(buf, &cursor)) {
                        return JsonTraceResult_NeedMoreInput;
                    }

                    u8 ch = buf.data[cursor];
                    if (ch != '{') {
                        return set_error(
                            parser,
                            "Invalid JSON Trace: expected '{' but got "
                            "'%c'",
                            ch);
                    }

                    parser->stack_cursor = 0;
                    push_stack(parser, '{');
                    parser->array_format.last_char = ch;

                    start = cursor;
                    cursor += 1;
                }

                bool found = false;
                while (cursor < buf.size && !found) {
                    u8 ch = buf.data[cursor++];
                    switch (ch) {
                        case '"': {
                            if (parser->array_format.last_char != '\\') {
                                if (is_stack_top(parser, '"')) {
                                    pop_stack(parser);
                                } else {
                                    push_stack(parser, '"');
                                }
                            }
                        } break;

                        case '{': {
                            if (!is_stack_top(parser, '"')) {
                                push_stack(parser, '{');
                            }
                        } break;

                        case '}': {
                            if (!is_stack_top(parser, '"')) {
                                pop_stack(parser);

                                if (is_stack_empty(parser)) {
                                    usize end = cursor;
                                    Buf trace_event;
                                    if (parser->buf_cursor) {
                                        save_input(parser, &parser->buf_cursor,
                                                   buf_slice(buf, start, end));
                                        trace_event = buf_slice(
                                            parser->buf, 0, parser->buf_cursor);
                                    } else {
                                        trace_event =
                                            buf_slice(buf, start, end);
                                    }
                                    handle_trace_event(trace_event);
                                    found = true;
                                }
                            }
                        } break;

                        default: {
                        } break;
                    }
                    parser->array_format.last_char = ch;
                }

                if (found) {
                    parser->state = State_ArrayFormat_AfterTraceEvent;
                } else {
                    save_input(parser, &parser->buf_cursor,
                               buf_slice(buf, start, buf.size));
                    return JsonTraceResult_NeedMoreInput;
                }
            } break;

            case State_ArrayFormat_AfterTraceEvent: {
                if (!skip_whitespace(buf, &cursor)) {
                    return JsonTraceResult_NeedMoreInput;
                }

                u8 ch = buf.data[cursor];
                if (ch == ',') {
                    cursor += 1;
                    parser->state = State_ArrayFormat;
                    parser->buf_cursor = 0;
                } else if (ch == ']') {
                    cursor += 1;
                    if (parser->has_object_format) {
                        parser->state = State_ObjectFormat_AfterValue;
                    } else {
                        parser->state = State_Done;
                        return JsonTraceResult_Done;
                    }
                } else {
                    return set_error(
                        parser,
                        "Invalid JSON Trace: expected ',' or ']' but got "
                        "'%c'",
                        ch);
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
                    case State_ObjectFormat_TraceEvents: {
                    } break;

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
