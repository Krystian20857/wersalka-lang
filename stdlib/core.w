func min(a, b) {
  if (a < b) {
    return a;
  }
  return b;
}

func max(a, b) {
  if (a > b) {
    return a;
  }
  return b;
}

func make_error(message) {
  var error = new {};
  error.message = message;
  return error;
}
