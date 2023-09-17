#include <emscripten.h>
#include <stdio.h>

#include "imgui.h"
#include "json_trace.h"
#include "src/json.h"
#include "src/memory.h"

struct App {
    MemoryArena arena;
    JsonTraceParser parser;
    Trace trace;
    bool is_loading;
    Buf input;
    usize input_size;
};

static usize INIT_INPUT_SIZE = 4096;

static void app_init(App *app) {
    *app = {};
    memory_arena_init(&app->arena);
    json_trace_parser_init(&app->parser, &app->arena);

    app->input.size = INIT_INPUT_SIZE;
    app->input.data = (u8 *)memory_alloc(INIT_INPUT_SIZE);
    ASSERT(app->input.data);
}

static bool app_is_loading(App *app) { return app->is_loading; }

static void app_begin_load(App *app) {
    ASSERT(!app_is_loading(app));
    app->is_loading = true;
}

static void *app_get_input_buffer(App *app, usize size) {
    ASSERT(app_is_loading(app));

    if (app->input.size < size) {
        while (app->input.size < size) {
            app->input.size <<= 1;
        }
        app->input.data =
            (u8 *)memory_realloc(&app->input.data, app->input.size);
        ASSERT(app->input.data);
    }
    app->input_size = size;
    return app->input.data;
}

static void app_submit_input(App *app) {
    ASSERT(app_is_loading(app));

    JsonTraceResult result = json_trace_parser_parse(
        &app->parser, &app->trace, buf_slice(app->input, 0, app->input_size));
    switch (result) {
        case JsonTraceResult_Error: {
            app->is_loading = false;
            printf("Error: %s\n", json_trace_parser_get_error(&app->parser));
        } break;
        case JsonTraceResult_Done: {
            app->is_loading = false;
        } break;
        case JsonTraceResult_NeedMoreInput: {
        } break;
        default:
            UNREACHABLE;
    }
}

static void app_end_load(App *app) {
    if (app_is_loading(app)) {
        app->is_loading = false;
    }
}

extern "C" {
EMSCRIPTEN_KEEPALIVE
void *app_new() {
    App *app = (App *)memory_alloc(sizeof(App));
    app_init(app);
    return app;
}

EMSCRIPTEN_KEEPALIVE
bool app_is_loading(void *app_) {
    App *app = (App *)app_;
    return app_is_loading(app);
}

EMSCRIPTEN_KEEPALIVE
void app_begin_load(void *app_) {
    App *app = (App *)app_;
    app_begin_load(app);
}

EMSCRIPTEN_KEEPALIVE
void *app_get_input_buffer(void *app_, usize size) {
    App *app = (App *)app_;
    return app_get_input_buffer(app, size);
}

EMSCRIPTEN_KEEPALIVE
void app_submit_input(void *app_) {
    App *app = (App *)app_;
    app_submit_input(app);
}

EMSCRIPTEN_KEEPALIVE
void app_end_load(void *app_) {
    App *app = (App *)app_;
    app_end_load(app);
}
}
