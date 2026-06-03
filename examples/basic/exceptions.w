func divide(a, b) {
  if (b == 0) {
    throw make_error("division by zero");
  }
  return a / b;
}

func safe_divide(a, b) {
  try {
    var result = divide(a, b);
    return result;
  } catch (var error) {
    print("Caught: {error.message}");
    return 0;
  }
}

func main() {
  print("10 / 2 = {safe_divide(10, 2)}");
  print("10 / 0 = {safe_divide(10, 0)}");

  try {
    divide(7, 0);
  } catch (var error) {
    print("Direct catch: {error.message}");
    var frame_count = len(error.stacktrace);
    print("Stack frames: {frame_count}");
    var frame_index = 0;
    while (frame_index < frame_count) {
      var frame = error.stacktrace[frame_index];
      print("  {frame.function_name} at line {frame.line}");
      frame_index += 1;
    }
  } finally {
    print("Finally block always runs");
  }
}
