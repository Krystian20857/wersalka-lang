module Counter {
  var n = 0;

  func bump() {
    n = n + 1;
    return n;
  }

  func value() {
    return n;
  }
}

func square(x) {
  Counter.bump();
  return x * x;
}

func cube(x) {
  return x * x * x;
}
