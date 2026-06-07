var core = import("core");

func make_pair() {
  return new (10, 20);
}

func make_triple_of(a, b, c) {
  return new (a, b, c);
}

func first_of(t) {
  return t[0];
}

func __main__() {
  var pair = new (1, 2);
  core.assert_eq(1, pair[0], "pair element 0");
  core.assert_eq(2, pair[1], "pair element 1");

  var triple = new (10, 20, 30);
  core.assert_eq(10, triple[0], "triple element 0");
  core.assert_eq(20, triple[1], "triple element 1");
  core.assert_eq(30, triple[2], "triple element 2");

  var mixed = new (1, "two", true);
  core.assert_eq(1, mixed[0], "mixed int element");
  core.assert_eq("two", mixed[1], "mixed string element");
  core.assert_eq(true, mixed[2], "mixed bool element");

  var (a, b) = pair;
  core.assert_eq(1, a, "destructure pair a");
  core.assert_eq(2, b, "destructure pair b");

  var (c, d, e) = triple;
  core.assert_eq(10, c, "destructure triple c");
  core.assert_eq(20, d, "destructure triple d");
  core.assert_eq(30, e, "destructure triple e");

  var nested = new (new (1, 2), 3);
  var ((f, g), h) = nested;
  core.assert_eq(1, f, "nested destructure f");
  core.assert_eq(2, g, "nested destructure g");
  core.assert_eq(3, h, "nested destructure h");

  var deep = new (1, new (2, new (3, 4)));
  var (i, (j, (k, l))) = deep;
  core.assert_eq(1, i, "deep destructure i");
  core.assert_eq(2, j, "deep destructure j");
  core.assert_eq(3, k, "deep destructure k");
  core.assert_eq(4, l, "deep destructure l");

  var from_call = make_pair();
  core.assert_eq(10, from_call[0], "tuple returned from function 0");
  core.assert_eq(20, from_call[1], "tuple returned from function 1");

  var (m, n) = make_pair();
  core.assert_eq(10, m, "destructure call result m");
  core.assert_eq(20, n, "destructure call result n");

  var built = make_triple_of(7, 8, 9);
  core.assert_eq(7, built[0], "tuple built from params 0");
  core.assert_eq(8, built[1], "tuple built from params 1");
  core.assert_eq(9, built[2], "tuple built from params 2");

  core.assert_eq(1, first_of(pair), "tuple passed as argument is indexable");
  core.assert_eq(10, first_of(make_pair()), "indexable through call chain");

  var arr = new [new (1, 2), new (3, 4), new (5, 6)];
  core.assert_eq(1, arr[0][0], "tuple inside array 0,0");
  core.assert_eq(4, arr[1][1], "tuple inside array 1,1");
  core.assert_eq(5, arr[2][0], "tuple inside array 2,0");

  var p = new (100, 200);
  var write_threw = false;
  try {
    p[0] = 999;
  } catch (var ex) {
    write_threw = true;
  }
  core.assert_eq(true, write_threw, "tuple write throws");
  core.assert_eq(100, p[0], "tuple still intact after failed write");

  var oob_threw = false;
  try {
    var x = pair[5];
  } catch (var ex) {
    oob_threw = true;
  }
  core.assert_eq(true, oob_threw, "out-of-bounds index throws");

  var outer = 42;
  var read_outer = func () {
    var (q, r) = new (outer, outer + 1);
    return q + r;
  };
  core.assert_eq(85, read_outer(), "destructure inside closure with capture");
}
