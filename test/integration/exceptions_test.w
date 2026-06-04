var error = import("error");

module Trace {
  var finally_runs = 0;
  var outer_catch_runs = 0;
  var inner_catch_runs = 0;

  func bump_finally() {
    finally_runs = finally_runs + 1;
  }

  func bump_outer_catch() {
    outer_catch_runs = outer_catch_runs + 1;
  }

  func bump_inner_catch() {
    inner_catch_runs = inner_catch_runs + 1;
  }
}

func divide(a, b) {
  if (b == 0) {
    throw error.make_error("division by zero");
  }
  return a / b;
}

func safe_divide(a, b) {
  try {
    return divide(a, b);
  } catch (var ex) {
    Trace.bump_outer_catch();
    return 0 - 1;
  }
}

func outer() {
  try {
    try {
      throw error.make_error("inner failure");
    } catch (var ex) {
      Trace.bump_inner_catch();
      throw error.make_error("rewrapped: {ex.message}");
    } finally {
      Trace.bump_finally();
    }
  } catch (var ex) {
    Trace.bump_outer_catch();
    return ex.message;
  }
  return "unreachable";
}

func __main__() {
  assert(divide(10, 2) == 5, "non-throwing path returns quotient");
  assert(safe_divide(10, 0) == 0 - 1, "catch handler returns sentinel");
  assert(Trace.outer_catch_runs == 1, "outer catch ran once for safe_divide");

  try {
    divide(7, 0);
    assert(0, "throw should have skipped this assert");
  } catch (var ex) {
    assert(ex.message == "division by zero",
           "caught exception carries make_error message");
  }

  var nested_message = outer();
  assert(nested_message == "rewrapped: inner failure",
         "nested catch sees rewrapped message");
  assert(Trace.inner_catch_runs == 1, "inner catch ran once in nested case");
  assert(Trace.finally_runs == 1, "finally ran exactly once in nested case");
  assert(Trace.outer_catch_runs == 2,
         "outer catch ran again for the rewrapped exception");

  var stack_frames = 0;
  try {
    divide(1, 0);
  } catch (var ex) {
    stack_frames = len(ex.stacktrace);
  }
  assert(stack_frames > 0, "stacktrace populated on caught exception");
}
