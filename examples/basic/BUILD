load("//bazel:defs.bzl", "wersalka_binary")

wersalka_binary(
    name = "hello",
    srcs = "hello.w",
)

wersalka_binary(
    name = "exceptions",
    srcs = "exceptions.w",
    deps = ["//stdlib"],
)
