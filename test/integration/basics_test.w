func fib(n) {
  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

func factorial(n) {
  if (n <= 1) {
    return 1;
  }
  return n * factorial(n - 1);
}

func __main__() {
  assert(1 + 2 == 3, "addition");
  assert(10 - 7 == 3, "subtraction");
  assert(4 * 5 == 20, "multiplication");
  assert(20 / 4 == 5, "division");

  assert(fib(0) == 0, "fib(0)");
  assert(fib(1) == 1, "fib(1)");
  assert(fib(10) == 55, "fib(10)");

  assert(factorial(5) == 120, "factorial(5)");

  var x = 0;
  var i = 0;
  while (i < 5) {
    x = x + i;
    i = i + 1;
  }
  assert(x == 10, "while loop sum 0..4");

  var n = fib(10);
  var s = "fib10={n}";
  assert(s == "fib10=55", "string interpolation");
}
