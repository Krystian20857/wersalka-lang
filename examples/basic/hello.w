func fib(n) {
  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

func square(input) {
  var result = new_array(len(input));
  var n = 0;
  while (n < len(input)) {
    result[n] = input[n] * input[n];
    n += 1;
  }
  return result;
}

func __main__() {
  print("Hello, World!");
  print("fib(10) = {fib(10)}");

  var ss = new [1, 2, 3, 4, 5];
  print("before: {ss}, after: {square(ss)}");
}
