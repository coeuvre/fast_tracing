#include <errno.h>
#include <stdio.h>
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

struct File {
    Buf path;
    FILE *handle;
};

static File open_file(Buf path) {
    File file = {};
    file.path = path;
    file.handle = fopen((char *)path.data, "rb");
    if (!file.handle) {
        fprintf(stderr, "Failed to open file %.*s: %s\n", (int)path.size,
                (char *)path.data, strerror(errno));
    }
    return file;
}

static void close_file(File *file) {
    if (file->handle) {
        fclose(file->handle);
        file->handle = 0;
    }
}

static usize read_file(File *file, Buf buf) {
    usize nread = fread(buf.data, 1, buf.size, file->handle);
    if (nread < buf.size) {
        int error = ferror(file->handle);
        if (error != 0) {
            nread = 0;
            fprintf(stderr, "Failed to read file %.*s: %s\n",
                    (int)file->path.size, (char *)file->path.data,
                    strerror(error));
        }
    }
    return nread;
}

static int run(Args args) {
    ASSERT(args.file.data);
    int result = 0;

    JsonTokenizer tok = json_init_tok();

    const usize kBufCap = 4096;
    u8 buf_[kBufCap];
    Buf buf = {.data = buf_, .size = kBufCap};

    File file = open_file(args.file);
    if (file.handle) {
        auto start = std::chrono::high_resolution_clock::now();
        usize nparsed = 0;

        while (json_is_scanning(&tok)) {
            usize nread = read_file(&file, buf);
            nparsed += nread;
            bool last_input = nread == 0;
            json_set_input(&tok, buf_slice(buf, 0, nread), last_input);

            bool eof = false;
            while (!eof) {
                JsonToken token = json_get_next_token(&tok);
                switch (token.type) {
                    case JsonToken_Error: {
                        fprintf(stderr, "JSON error: %.*s\n",
                                (int)token.value.size,
                                (char *)token.value.data);
                        eof = true;
                    } break;

                    case JsonToken_Eof: {
                        eof = true;
                    } break;

                    default: {
                    } break;
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        duration.count();
        f64 speed =
            (f64)nparsed / (f64)duration.count() * 1024.0 * 1024.0 / 1'000'000;
        fprintf(stdout, "Speed: %.2f MB/s\n", speed);

        json_deinit_tok(&tok);

        close_file(&file);
    } else {
        result = 1;
    }

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