func basic_closure() {
  var a = 5;
  var f = func (b) {
    return a + b;
  };
  print("f(4) = {f(4)}");
}

func nested_closure() {
  var c = 10;
  var d = 10;
  var f = func () {
    var h = 1;
    return func(a, b) { return h + a * d + b * c; };
  };

  var a = 2;
  var b = 3;
  print("a = {a}, b = {b}, c = {c}, d = {d}");
  print("a + b * c = {f()(a, b)}");
}

func array_of_closures() {
  var size = 10;
  var array = new_array(size);
  var n = 0;
  while (n < size) {
    array[n] = func () {
      # captured `n`
      return n * 2;
    };
    n += 1;
  }

  n = 0;
  while (n < len(array)) {
    print("array[{n}]() = {array[n]()}");
    n += 1;
  }
}

func run_wrapped(name, f) {
  print("Example `{name}`");
  f();
  print("");
}

func __func() { }
var __closure = func () {};

func function_and_closure() {
  print("{type(__func)}, {type(__closure)}");
}

func __main__() {
  run_wrapped("basic closure", basic_closure);
  run_wrapped("nested closure", nested_closure);
  run_wrapped("array of closures", array_of_closures);
  run_wrapped("function and closure", function_and_closure);
}
