var error = import("error");

module Trace {
  var finally_runs = 0;

  func bump_finally() {
    finally_runs = finally_runs + 1;
  }
}

func sum_until(limit) {
  var total = 0;
  var i = 0;
  while (i < 100) {
    if (i == limit) {
      break;
    }
    total = total + i;
    i += 1;
  }
  return total;
}

func sum_evens(limit) {
  var total = 0;
  var i = 0;
  while (i <= limit) {
    if (i % 2 == 1) {
      i += 1;
      continue;
    }
    total = total + i;
    i += 1;
  }
  return total;
}

func nested_break_count() {
  var seen = 0;
  var i = 0;
  while (i < 3) {
    var j = 0;
    while (j < 5) {
      if (j == 2) {
        break;
      }
      seen = seen + 1;
      j = j + 1;
    }
    i += 1;
  }
  return seen;
}

func nested_continue_sum() {
  var total = 0;
  var i = 0;
  while (i < 3) {
    i += 1;
    var j = 0;
    while (j < 4) {
      j = j + 1;
      if (j == 2) {
        continue;
      }
      total = total + j;
    }
  }
  return total;
}

func break_through_finally() {
  var i = 0;
  while (i < 10) {
    try {
      if (i == 3) {
        break;
      }
      i += 1;
    } finally {
      Trace.bump_finally();
    }
  }
  return i;
}

func continue_through_finally() {
  var iterations = 0;
  var i = 0;
  while (i < 4) {
    i += 1;
    try {
      if (i % 2 == 0) {
        continue;
      }
      iterations = iterations + 1;
    } finally {
      Trace.bump_finally();
    }
  }
  return iterations;
}

func __main__() {
  assert(sum_until(5) == 10, "break exits the loop at i == 5");
  assert(sum_until(0) == 0, "break on first iteration yields zero");

  assert(sum_evens(6) == 12, "continue skips odd numbers (0+2+4+6)");

  assert(nested_break_count() == 6,
         "inner break only escapes inner loop (3 outer * 2 inner)");

  assert(nested_continue_sum() == 24,
         "inner continue only re-iterates inner loop (3 * (1+3+4))");

  Trace.finally_runs = 0;
  assert(break_through_finally() == 3, "break runs after finally executed");
  assert(Trace.finally_runs == 4,
         "finally ran for each of 4 iterations including the breaking one");

  Trace.finally_runs = 0;
  assert(continue_through_finally() == 2,
         "continue path still updates loop accumulator after finally");
  assert(Trace.finally_runs == 4,
         "finally ran once per iteration when continuing");
}
