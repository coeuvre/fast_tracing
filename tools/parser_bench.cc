#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>

#include "src/json.h"
#include "tools/common.h"

const char *USAGE = R"(parser_bench

Benchmark the parser with the given trace file.

USAGE:
    parser_bench [OPTIONS] <FILE>

OPTIONS:
    -h, --help                  Print help information.
)";

static void print_usage() { fprintf(stderr, "%s", USAGE); }

struct Args {
    bool valid;
    bool help;
    Buf file;
};

static void parse_arg(Args *args, Buf key, Buf value) {
    if (buf_equal(key, STR_LITERAL("-h")) ||
        buf_equal(key, STR_LITERAL("--help"))) {
        args->help = true;
    } else if (!value.data) {
        // Arg without value, treat it as <FILE> argument.
        if (!args->file.data) {
            args->file = key;
        } else {
            // Saw a <FILE> argument before.
            args->valid = false;
        }
    } else {
        args->valid = false;
    }
}

static Args parse_args(int argc, char *argv[]) {
    Args args = {.valid = true};
    for (int i = 1; i < argc; ++i) {
        Buf key, value;
        split_arg(argv[i], &key, &value);
        parse_arg(&args, key, value);
    }
    if (!args.file.data) {
        args.valid = false;
    }
    return args;
}

static Buf read_all(FILE *file) {
    ASSERT(file);

    int r = fseek(file, 0, SEEK_END);
    ASSERT(r == 0);
    isize offset = ftell(file);
    ASSERT(offset >= 0);
    usize size = (usize)offset;

    r = fseek(file, 0, SEEK_SET);
    ASSERT(r == 0);

    void *buf = malloc(size);
    usize nread = fread(buf, 1, size, file);
    ASSERT(nread == size);

    return {.data = (u8 *)buf, .size = size};
}

struct JsonInputContext {
    Buf content;
    bool eof;
};

bool json_input_fetch(void *ctx_, MemoryArena *arena, Buf *buf,
                      JsonError *error) {
    JsonInputContext *ctx = (JsonInputContext *)ctx_;
    if (ctx->eof) {
        return false;
    }
    *buf = ctx->content;
    ctx->eof = true;
    return true;
}

static int run(Args args) {
    ASSERT(args.file.data);
    int result = 0;

    FILE *file = fopen((const char *)args.file.data, "rb");
    Buf content = read_all(file);

    JsonInputContext ctx = {
        .content = content,
        .eof = false,
    };
    JsonInput input;
    json_input_init(&input, &ctx, json_input_fetch);

    MemoryArena arena;
    memory_arena_init(&arena);

    auto start = std::chrono::high_resolution_clock::now();
    usize nparsed = content.size;

    JsonToken token;
    JsonError error;
    while (json_scan(&arena, &input, &token, &error)) {
        memory_arena_clear(&arena);
    }

    if (error.has_error) {
        printf("Error: %.*s\n", (int)error.message.size, error.message.data);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    duration.count();
    f64 speed =
        (f64)nparsed / (f64)duration.count() * 1024.0 * 1024.0 / 1'000'000;
    fprintf(stdout, "Speed: %.2f MB/s\n", speed);

    return result;
}

int main(int argc, char *argv[]) {
    int result = 0;
    Args args = parse_args(argc, argv);
    if (args.valid && !args.help) {
        result = run(args);
    } else {
        print_usage();
        result = 1;
    }
    return result;
}