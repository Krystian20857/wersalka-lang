func fib(n) {
  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

func __main__() {
  print("Hello, World!");
  print("fib(10) = {fib(10)}");
}
