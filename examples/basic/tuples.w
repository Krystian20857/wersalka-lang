func __main__() {
  var t = new (new (1, 2), new (3, 4));
  var ((a, b), (c, d)) = t;
  var f = func () { return c; };
  print("a = {a}, b = {b}, c = {c}, d = {d}, c (captured) = {f()}");
}
