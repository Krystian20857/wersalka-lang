load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "runtime",
    srcs = [
        "ast.cc",
        "diagnostic.cc",
        "driver.cc",
        "parser.cc",
        "source.cc",
        "tokenizer.cc",
        "zone.cc",
    ],
    hdrs = [
        "ast.h",
        "diagnostic.h",
        "driver.h",
        "parser.h",
        "source.h",
        "tokenizer.h",
        "zone.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/base:config",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/base:prefetch",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:flat_hash_set",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/log:initialize",
        "@abseil-cpp//absl/log:log",
        "@abseil-cpp//absl/strings:charset",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_binary(
    name = "playground",
    srcs = ["playground.cc"],
    deps = ["//runtime"],
)
