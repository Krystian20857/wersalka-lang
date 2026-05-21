load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
    name = "text_fixtures",
    srcs = ["test_fixtures.cc"],
    hdrs = ["test_fixtures.h"],
    deps = [
        "//runtime",
        "@googletest//:gtest",
    ],
)

cc_test(
    name = "zone_test",
    size = "small",
    srcs = [
        "zone_test.cc",
    ],
    deps = [
        "//runtime",
        "//test:text_fixtures",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "tokenizer_test",
    size = "small",
    srcs = [
        "tokenizer_test.cc",
    ],
    deps = [
        "//runtime",
        "//test:text_fixtures",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "parser_test",
    size = "small",
    srcs = [
        "parser_test.cc",
    ],
    deps = [
        "//runtime",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
