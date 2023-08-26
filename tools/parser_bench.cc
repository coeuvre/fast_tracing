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
  if (BufEql(key, STR_LITERAL("-h")) || BufEql(key, STR_LITERAL("--help"))) {
    args->help = true;
  } else if (!value.ptr) {
    // Arg without value, treat it as <FILE> argument.
    if (!args->file.ptr) {
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
  if (!args.file.ptr) {
    args.valid = false;
  }
  return args;
}

static bool ReadFile(Buf filename, FILE *file, Buf buf, Buf *out_content) {
  ASSERT(out_content);
  bool has_content = true;
  usize nread = fread((void *)buf.ptr, 1, buf.len, file);
  if (nread < buf.len) {
    int error = ferror(file);
    if (error != 0) {
      has_content = false;
      fprintf(stderr, "Failed to read file %.*s: %s\n", (int)filename.len,
              filename.ptr, strerror(error));
    }
  }

  has_content = has_content && nread > 0;
  if (has_content) {
    *out_content = {.ptr = buf.ptr, .len = nread};
  } else {
    *out_content = {};
  }
  return has_content;
}

static int ParseFile(Buf filename, FILE *file) {
  int result = 0;

  const usize kBufCap = 4096;
  u8 buf_[kBufCap];
  Buf buf = {.ptr = buf_, .len = kBufCap};

  Buf content;
  while (ReadFile(filename, file, buf, &content)) {
    // TODO: Parse the content.
  }

  return result;
}

static int Run(Args args) {
  ASSERT(args.file.ptr);
  int result = 0;

  char *filename = (char *)args.file.ptr;
  FILE *file = fopen(filename, "r");
  if (file) {
    ParseFile(args.file, file);
  } else {
    result = errno;
    fprintf(stderr, "Failed to open file %.*s: %s\n", (int)args.file.len,
            args.file.ptr, strerror(errno));
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