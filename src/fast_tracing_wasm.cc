#include <emscripten.h>
#include <pthread.h>
#include <stdio.h>

#include "imgui.h"
#include "src/json.h"
#include "src/memory.h"

struct LoadingState {
    pthread_t thread;

    pthread_mutex_t mutex;
    // guarded by `mutex` {
    pthread_cond_t cond_consume;
    pthread_cond_t cond_produce;
    Buf input;
    usize input_size;
    bool using_input;
    bool loading;
    // }
};

struct App {
    LoadingState loading;
};

static bool json_input_fetch(void *ctx_, MemoryArena *arena, Buf *buf,
                             JsonError *error) {
    App *app = (App *)ctx_;

    pthread_mutex_lock(&app->loading.mutex);
    if (app->loading.using_input) {
        app->loading.using_input = false;
        pthread_cond_signal(&app->loading.cond_produce);
    }

    pthread_cond_wait(&app->loading.cond_consume, &app->loading.mutex);
    ASSERT(app->loading.using_input);
    pthread_mutex_unlock(&app->loading.mutex);

    *buf = {.data = app->loading.input.data, .size = app->loading.input_size};
    return buf->size > 0;
}

static void *app_loading_thread_start(void *args) {
    App *app = (App *)args;

    pthread_mutex_lock(&app->loading.mutex);
    app->loading.using_input = false;
    app->loading.loading = true;
    pthread_mutex_unlock(&app->loading.mutex);

    JsonInput input;
    json_input_init(&input, app, json_input_fetch);

    MemoryArena arena;
    memory_arena_init(&arena);
    JsonToken token;
    JsonError error;
    while (json_scan(&arena, &input, &token, &error)) {
    }

    if (error.has_error) {
        printf("Error: %.*s\n", (int)error.message.size, error.message.data);
    }

    pthread_mutex_lock(&app->loading.mutex);
    app->loading.using_input = false;
    app->loading.loading = false;
    pthread_cond_signal(&app->loading.cond_produce);
    pthread_mutex_unlock(&app->loading.mutex);

    return 0;
}

static void app_init(App *app) {
    *app = {};
    pthread_mutex_init(&app->loading.mutex, 0);
    pthread_cond_init(&app->loading.cond_consume, 0);
    pthread_cond_init(&app->loading.cond_produce, 0);
}

static bool app_is_loading(App *app) { return app->loading.thread != 0; }

static void app_start_loading(App *app) {
    ASSERT(!app_is_loading(app));
    int ret =
        pthread_create(&app->loading.thread, 0, app_loading_thread_start, app);
    ASSERT(ret == 0);
}

static void *app_lock_input(App *app, usize size) {
    ASSERT(app_is_loading(app));

    pthread_mutex_lock(&app->loading.mutex);
    if (app->loading.using_input) {
        pthread_cond_wait(&app->loading.cond_produce, &app->loading.mutex);
    }

    ASSERT(!app->loading.using_input);
    app->loading.using_input = true;

    if (app->loading.loading && size > 0) {
        if (app->loading.input.size < size) {
            app->loading.input.data =
                (u8 *)memory_realloc(app->loading.input.data, size);
            app->loading.input.size = size;
        }

        ASSERT(app->loading.input.data && app->loading.input.size >= size);
        app->loading.input_size = size;
    } else {
        app->loading.input_size = 0;
    }

    void *ptr;
    if (app->loading.input_size > 0) {
        ptr = app->loading.input.data;
    } else {
        ptr = 0;
    }
    return ptr;
}

static void app_unlock_input(App *app) {
    pthread_mutex_unlock(&app->loading.mutex);

    pthread_cond_signal(&app->loading.cond_consume);

    if (app->loading.input_size == 0) {
        pthread_join(app->loading.thread, 0);
        app->loading.thread = 0;
    }
}

static void app_on_chunk_done(App *app) {
    app_lock_input(app, 0);
    app_unlock_input(app);
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
void app_start_loading(void *app_) {
    App *app = (App *)app_;
    app_start_loading(app);
}

EMSCRIPTEN_KEEPALIVE
void *app_lock_input(void *app_, usize size) {
    App *app = (App *)app_;
    return app_lock_input(app, size);
}

EMSCRIPTEN_KEEPALIVE
void app_unlock_input(void *app_) {
    App *app = (App *)app_;
    app_unlock_input(app);
}

EMSCRIPTEN_KEEPALIVE
void app_on_chunk_done(void *app_) {
    App *app = (App *)app_;
    app_on_chunk_done(app);
}
}