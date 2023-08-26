load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "emsdk",
    sha256 = "55d930c9daead98d27afab71e0389756371cbf3df5dfa2d5dc2805ab4e43d921",
    strip_prefix = "emsdk-c8dcb45665fec5e743336703f1d22a191810f2a8/bazel",
    url = "https://github.com/emscripten-core/emsdk/archive/c8dcb45665fec5e743336703f1d22a191810f2a8.tar.gz",
)

load("@emsdk//:deps.bzl", emsdk_deps = "deps")
emsdk_deps()

load("@emsdk//:emscripten_deps.bzl", emsdk_emscripten_deps = "emscripten_deps")
emsdk_emscripten_deps(emscripten_version = "3.1.45")

load("@emsdk//:toolchains.bzl", "register_emscripten_toolchains")
register_emscripten_toolchains()

http_archive(
    name = "imgui",
    sha256 = "5049c9d44280dcda1921c79dc054fe7a82aecebb8f27d2f933494069323d60ca",
    strip_prefix = "imgui-f8704cd085c4347f835c21dc12a3951924143872",
    url = "https://github.com/ocornut/imgui/archive/f8704cd085c4347f835c21dc12a3951924143872.tar.gz",
    build_file = "@//third_party/imgui:BUILD",
)