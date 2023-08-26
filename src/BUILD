package(default_visibility = ["//visibility:public"])

load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

cc_library(
    name = "common",
    hdrs = ["common.h"],
    srcs = ["common.cc"],
)

cc_binary(
    name = "fast_tracing",
    srcs = select({
        "@platforms//cpu:wasm32": ["fast_tracing_wasm.cc"],
        "//conditions:default": ["fast_tracing.cc"],
    }),
    deps = [
        "@imgui",
    ],
    linkopts = select({
        "@platforms//cpu:wasm32": ["-sMODULARIZE -sEXPORT_ES6 -sEXPORTED_RUNTIME_METHODS=cwrap"],
        "//conditions:default": [],
    }),
)

wasm_cc_binary(
    name = "fast_tracing_wasm",
    cc_target = ":fast_tracing",
    outputs = ["fast_tracing.wasm", "fast_tracing.js"],
)