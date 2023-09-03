cc_library(
    name = "common",
    hdrs = ["common.h"],
    srcs = ["common.cc"],
    deps = ["//src:common"],
)

cc_binary(
    name = "trace_gen",
    srcs = ["trace_gen.cc"],
    deps = [":common"],
)

cc_binary(
    name = "parser_bench",
    srcs = ["parser_bench.cc"],
    deps = [
        ":common",
        "//src:json",
    ],
)