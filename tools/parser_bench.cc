#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>

#include "src/json_trace.h"
#include "src/trace.h"
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

// struct JsonInputContext {
//     FILE *file;
//     Buf buf;
//     usize nread;
// };

// bool json_input_fetch(void *ctx_, MemoryArena *arena, Buf *buf,
//                       JsonError *error) {
//     JsonInputContext *ctx = (JsonInputContext *)ctx_;

//     usize nread = fread((void *)ctx->buf.data, 1, ctx->buf.size, ctx->file);
//     if (nread == 0) {
//         return false;
//     }
//     *buf = buf_slice(ctx->buf, 0, nread);
//     ctx->nread += nread;
//     return true;
// }

static int run(Args args) {
    ASSERT(args.file.data);

    FILE *file = fopen((const char *)args.file.data, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file %.*s: %s\n", (int)args.file.size,
                args.file.data, strerror(errno));
        return 1;
    }

    u8 buf[4096];

    MemoryArena arena;
    memory_arena_init(&arena);

    JsonTraceParser parser;
    json_trace_parser_init(&parser, &arena);

    Trace trace;
    // TODO: init trace

    auto start = std::chrono::high_resolution_clock::now();

    usize total = 0;
    bool done = false;
    while (!done) {
        usize nread = fread(buf, 1, sizeof(buf), file);
        if (nread == 0) {
            break;
        }

        total += nread;

        JsonTraceResult result = json_trace_parser_parse(
            &parser, &trace, {.data = buf, .size = nread});
        switch (result) {
            case JsonTraceResult_Error: {
                fprintf(stderr, "Error: %s\n",
                        json_trace_parser_get_error(&parser));
                return 1;
            }
            case JsonTraceResult_Done: {
                done = true;
            } break;
            case JsonTraceResult_NeedMoreInput: {
                break;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    duration.count();
    f64 speed =
        (f64)total / (f64)duration.count() * 1024.0 * 1024.0 / 1'000'000;
    fprintf(stdout, "Speed: %.2f MB/s\n", speed);

    memory_arena_deinit(&arena);

    return 0;
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