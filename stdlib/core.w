module core {
  var error = import("error");

  func assert_eq(expected, actual, message) {
    if (expected != actual) {
      throw error.make_error("Assertion failed, expected `{expected}` got `{actual}`");
    }
  }
}
