#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools/common.h"

// Chrome Trace Event Format
// https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview

static const char *USAGE = R"(trace_gen

Generates a (large) trace file for benchmarking purposes.

USAGE:
    trace_gen [OPTIONS]

OPTIONS:
    -h, --help                  Print help information.
    -o, --out=<FILE>            Write output to <FILE>. Default: stdout
    --seed=<INT>                Set a seed for the random number generator. Default: 0
)";

const usize MAX_THREADS = 20;
const usize MAX_STACK_DEPTH = 6;
const usize MAX_FUNCTION_CALLS = 10;
const usize MAX_DELAY = 1000000;

static void print_usage() { fprintf(stderr, "%s", USAGE); }

// Small PRNG described by https://burtleburtle.net/bob/rand/smallprng.html
struct RandomSeries {
    u64 a, b, c, d;
};

static u64 rotate_left(u64 value, int shift) {
    return ((value << shift) | (value >> (64 - shift)));
}

static u64 random_u64(RandomSeries *x) {
    u64 e = x->a - rotate_left(x->b, 27);
    x->a = x->b ^ rotate_left(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

static u64 random_between_u64(RandomSeries *x, u64 a, u64 b) {
    if (a >= b) {
        return a;
    }
    u64 count = (b - a) + 1;
    return a + random_u64(x) % count;
}

static RandomSeries init_random_series(u64 seed) {
    RandomSeries x = {};
    x.a = 0xf1ea5eed;
    x.b = seed;
    x.c = seed;
    x.d = seed;
    for (u64 i = 0; i < 20; ++i) {
        random_u64(&x);
    }
    return x;
}

struct Args {
    bool valid;
    bool help;
    Buf out;
    u64 seed;
};

static void parse_arg(Args *args, Buf key, Buf value) {
    if (buf_equal(key, STR_LITERAL("-h")) ||
        buf_equal(key, STR_LITERAL("--help"))) {
        args->help = true;
    } else if (buf_equal(key, STR_LITERAL("-o")) ||
               buf_equal(key, STR_LITERAL("--out"))) {
        args->out = value;
    } else if (buf_equal(key, STR_LITERAL("--seed"))) {
        if (sscanf((const char *)value.data, "%" SCNu64, &args->seed) != 1) {
            args->valid = false;
        }
    } else {
        args->valid = false;
    }
}

static Args parse_args(int argc, char *argv[]) {
    Args result = {.valid = true};
    for (int i = 1; i < argc; ++i) {
        Buf key, value;
        split_arg(argv[i], &key, &value);
        parse_arg(&result, key, value);
    }
    return result;
}

static void generate_function(FILE *out, RandomSeries *series, usize thread_id,
                              u64 *current_time_us, usize current_depth,
                              usize max_depth, usize func_index) {
    if (current_depth > max_depth) {
        return;
    }

    u64 ts = *current_time_us;

    u64 pre_call_delay = random_between_u64(series, 0, MAX_DELAY);
    *current_time_us += pre_call_delay;

    u64 max_function_calls = random_between_u64(series, 1, MAX_FUNCTION_CALLS);
    for (usize func_index = 0; func_index < max_function_calls; ++func_index) {
        generate_function(out, series, thread_id, current_time_us,
                          current_depth + 1, max_depth, func_index);
        u64 call_delay = random_between_u64(series, 0, MAX_DELAY);
        *current_time_us += call_delay;
    }

    u64 post_call_delay = random_between_u64(series, 0, MAX_DELAY);
    *current_time_us += post_call_delay;

    u64 dur = *current_time_us - ts;

    if (current_depth > 0) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "F(%zu, %zu, %zu)", thread_id, current_depth,
                 func_index);
        fprintf(
            out,
            "{\"name\": \"%s\", \"cat\": \"Unknown\", \"ph\": \"X\", \"ts\": "
            "%" PRIu64 ", \"dur\": %" PRIu64 ", \"tid\": %zu, \"pid\": 1 }",
            buf, ts, dur, thread_id);
    }
}

static void generate_thread(FILE *out, RandomSeries *series, usize thread_id) {
    u64 current_time_us = random_between_u64(series, 0, MAX_DELAY);
    usize max_depth = random_between_u64(series, 1, MAX_STACK_DEPTH);

    generate_function(out, series, thread_id, &current_time_us, 0, max_depth,
                      0);
}

static void generate(FILE *out, u64 seed) {
    RandomSeries series_ = init_random_series(seed);
    RandomSeries *series = &series_;

    usize num_threads = random_between_u64(series, 1, MAX_THREADS);

    fprintf(out, "{\"traceEvents\":[\n");

    for (usize thread_index = 0; thread_index < num_threads; ++thread_index) {
        usize thread_id = thread_index + 1;
        generate_thread(out, series, thread_id);
    }

    fprintf(out, "]}\n");
}

static int run(Args args) {
    int result = 0;
    FILE *out = stdout;
    if (args.out.data) {
        out = fopen((char *)args.out.data, "w");
        if (!out) {
            result = errno;
            fprintf(stderr, "Failed to open file %.*s: %s\n",
                    (int)args.out.size, (char *)args.out.data, strerror(errno));
        }
    }

    if (out) {
        generate(out, args.seed);

        if (out != stdout) {
            fclose(out);
        }
    }

    return result;
}

int main(int argc, char *argv[]) {
    int result = 0;

    auto args = parse_args(argc, argv);
    if (args.valid && !args.help) {
        result = run(args);
    } else {
        print_usage();
        result = 1;
    }

    return result;
}