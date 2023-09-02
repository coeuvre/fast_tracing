#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "tools/common.h"

const char *kUsage = R"(parser_bench

Benchmark the parser with the given trace file.

USAGE:
    parser_bench [OPTIONS] <FILE>

OPTIONS:
    -h, --help                  Print help information.
)";

static void PrintUsage() { fprintf(stderr, "%s", kUsage); }

struct Args {
  bool valid;
  bool help;
  Buf file;
};

static void ParseArg(Args *args, Buf key, Buf value) {
  if (BufAreEqual(key, STR_LITERAL("-h")) ||
      BufAreEqual(key, STR_LITERAL("--help"))) {
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

static Args ParseArgs(int argc, char *argv[]) {
  Args args = {.valid = true};
  for (int i = 1; i < argc; ++i) {
    Buf key, value;
    SplitArg(argv[i], &key, &value);
    ParseArg(&args, key, value);
  }
  if (!args.file.data) {
    args.valid = false;
  }
  return args;
}

struct JsonTokenizer {
  Buf input;
  bool has_error;
};

static bool IsScanning(JsonTokenizer *tok) { return !tok->has_error; }

static void SetInput(JsonTokenizer *tok, Buf input) { tok->input = input; }

enum JsonTokenType {
  kJsonTokenError,
  kJsonTokenEof,
};

struct JsonToken {
  JsonTokenType type;
  union {
    Buf error;
    Buf string;
    Buf number;
  };
};

static JsonToken GetJsonToken(JsonTokenizer *tok) {
  JsonToken token = {};
  if (token.type == kJsonTokenError) {
    tok->has_error = true;
  }
  return token;
}

struct File {
  Buf path;
  FILE *handle;
};

static File OpenFile(Buf path) {
  File file = {};
  file.path = path;
  file.handle = fopen((char *)path.data, "rb");
  if (!file.handle) {
    fprintf(stderr, "Failed to open file %.*s: %s\n", (int)path.size,
            (char *)path.data, strerror(errno));
  }
  return file;
}

static void CloseFile(File *file) {
  if (file->handle) {
    fclose(file->handle);
    file->handle = 0;
  }
}

static usize ReadFile(File *file, Buf buf) {
  usize nread = fread(buf.data, 1, buf.size, file->handle);
  if (nread < buf.size) {
    int error = ferror(file->handle);
    if (error != 0) {
      nread = 0;
      fprintf(stderr, "Failed to read file %.*s: %s\n", (int)file->path.size,
              (char *)file->path.data, strerror(error));
    }
  }
  return nread;
}

static int Run(Args args) {
  ASSERT(args.file.data);
  int result = 0;

  JsonTokenizer tok = {};

  const usize kBufCap = 4096;
  u8 buf_[kBufCap];
  Buf buf = {.data = buf_, .size = kBufCap};

  File file = OpenFile(args.file);
  if (file.handle) {
    while (IsScanning(&tok)) {
      usize nread = ReadFile(&file, buf);
      if (nread == 0) {
        break;
      }

      SetInput(&tok, GetSubBuf(buf, 0, nread));
      bool done = false;
      while (!done) {
        JsonToken token = GetJsonToken(&tok);
        switch (token.type) {
          case kJsonTokenError:
            fprintf(stderr, "JSON error: %.*s\n", (int)token.error.size,
                    (char *)token.error.data);
            done = true;
            break;

          case kJsonTokenEof:
            done = true;
            break;

          default:
            UNREACHABLE;
        }
      }
    }

    CloseFile(&file);
  } else {
    result = 1;
  }

  return result;
}

int main(int argc, char *argv[]) {
  int result = 0;
  Args args = ParseArgs(argc, argv);
  if (args.valid && !args.help) {
    result = Run(args);
  } else {
    PrintUsage();
    result = 1;
  }
  return result;
}