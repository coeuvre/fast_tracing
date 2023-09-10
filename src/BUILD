package(default_visibility = ["//visibility:public"])

load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

cc_library(
    name = "common",
    hdrs = ["defs.h", "memory.h", "buf.h"],
    srcs = ["buf.cc", "memory.cc"],
    linkstatic = True,
)

cc_test(
    name = "common_test",
    size = "small",
    srcs = ["memory_test.cc"],
    deps = [
      ":common",
      "@com_google_googletest//:gtest_main",
    ],
    linkstatic = True,
)

cc_library(
    name = "json",
    hdrs = ["json.h"],
    srcs = ["json.cc"],
    deps = [
        ":common",
    ],
    linkstatic = True,
)

cc_test(
    name = "json_test",
    size = "small",
    srcs = ["json_test.cc"],
    deps = [
      ":json",
      "@com_google_googletest//:gtest_main",
    ],
    linkstatic = True,
)

cc_binary(
    name = "fast_tracing",
    srcs = select({
        "@platforms//cpu:wasm32": ["fast_tracing_wasm.cc"],
        "//conditions:default": ["fast_tracing.cc"],
    }),
    deps = [
        ":common",
        ":json",
        "@imgui",
    ],
    copts = select({
        "@platforms//cpu:wasm32": ["-pthread"],
        "//conditions:default": [],
    }),
    linkopts = select({
        "@platforms//cpu:wasm32": [
            "-sMODULARIZE",
            "-sEXPORT_ES6",
            "-sEXPORTED_RUNTIME_METHODS=cwrap",
            "-sASSERTIONS", 
            "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency",
            "-pthread",
        ],
        "//conditions:default": [],
    }),
)

wasm_cc_binary(
    name = "fast_tracing_wasm",
    cc_target = ":fast_tracing",
    outputs = [
        "fast_tracing.wasm",
        "fast_tracing.js",
        "fast_tracing.worker.js",
    ],
)
