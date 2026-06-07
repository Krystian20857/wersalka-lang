var core = import("core");

func __main__() {
  var a = 5;
  var add_a = func (x) { return x + a; };
  core.assert_eq(15, add_a(10), "capture single local");

  a = 7;
  core.assert_eq(17, add_a(10), "captured var follows reassignment");

  var b = 3;
  var c = 4;
  var combine = func (x) { return x * b + c; };
  core.assert_eq(10, combine(2), "multiple captures");

  var counter = 0;
  var inc = func () { counter = counter + 1; return counter; };
  core.assert_eq(1, inc(), "counter first call");
  core.assert_eq(2, inc(), "counter second call");
  core.assert_eq(3, inc(), "counter third call");
  core.assert_eq(3, counter, "captured counter mutated");

  var d = 10;
  var make_adder = func () {
    var e = 1;
    return func (x) { return x + d + e; };
  };
  var adder = make_adder();
  core.assert_eq(13, adder(2), "nested closure 2 + 10 + 1");

  var f = 100;
  var make = func (g) {
    return func (h) { return f + g + h; };
  };
  var k1 = make(1);
  var k2 = make(20);
  core.assert_eq(104, k1(3), "k1 uses g=1");
  core.assert_eq(123, k2(3), "k2 uses g=20");
  core.assert_eq(108, k1(7), "k1 still uses g=1");

  var bind = func (p) {
    return func (q) { return p + q; };
  };
  var add5 = bind(5);
  var add9 = bind(9);
  core.assert_eq(7, add5(2), "bind add5");
  core.assert_eq(11, add9(2), "bind add9");
  core.assert_eq(105, add5(100), "bind add5 again");

  var m = 0;
  {
    var n = 50;
    var read_n = func () { return n; };
    m = read_n();
  }
  core.assert_eq(50, m, "block-scoped capture");

  var i = 0;
  var get_i = func () { return i; };
  while (i < 4) {
    i = i + 1;
  }
  core.assert_eq(4, get_i(), "closure sees final loop value");
}
