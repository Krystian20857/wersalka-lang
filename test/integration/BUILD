load("//bazel:defs.bzl", "wersalka_test")

wersalka_test(
    name = "basics",
    srcs = "basics_test.w",
    deps = ["//stdlib"],
)

wersalka_test(
    name = "modules",
    srcs = "modules_test.w",
)

wersalka_test(
    name = "imports_test",
    srcs = "imports/main.w",
    data = ["imports/util/math.w"],
    deps = ["//stdlib"],
)

wersalka_test(
    name = "exceptions",
    srcs = "exceptions_test.w",
    deps = ["//stdlib"],
)

wersalka_test(
    name = "arrays",
    srcs = "arrays_test.w",
    deps = ["//stdlib"],
)

wersalka_test(
    name = "builtins",
    srcs = "builtins_test.w",
)

wersalka_test(
    name = "closures",
    srcs = "closures_test.w",
    deps = ["//stdlib"],
)

wersalka_test(
    name = "tuples",
    srcs = "tuples_test.w",
    deps = ["//stdlib"],
)

wersalka_test(
    name = "loops",
    srcs = "loops_test.w",
    deps = ["//stdlib"],
)
