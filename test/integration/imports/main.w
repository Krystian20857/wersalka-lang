func __main__() {
  var math = import("util.math");

  assert(math.square(7) == 49, "imported function returns correct value");
  assert(math.cube(3) == 27, "second imported function works");
  assert(math.Counter.value() == 1, "side effect from square recorded");

  assert(math.square(2) == 4, "imported function callable again");
  assert(math.Counter.value() == 2, "side effect accumulates");

  var again = import("util.math");
  assert(again.Counter.value() == 2,
         "second import is a cache hit, state preserved");

  again.Counter.bump();
  assert(math.Counter.value() == 3,
         "both bindings reference the same cached module");
}
