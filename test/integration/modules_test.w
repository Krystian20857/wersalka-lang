module Math {
  var pi_approx = 3;

  func double(x) {
    return x + x;
  }

  func quadruple(x) {
    return double(double(x));
  }
}

module Outer {
  var top = 42;

  module Mid {
    module Inner {
      func reach_top() {
        return Outer.top;
      }
    }

    func bump_top() {
      Outer.top = Outer.top + 1;
      return Outer.top;
    }
  }
}

module Parity {
  func is_even(n) {
    if (n == 0) {
      return 1;
    }
    return is_odd(n - 1);
  }

  func is_odd(n) {
    if (n == 0) {
      return 0;
    }
    return is_even(n - 1);
  }
}

module Counter {
  var n = 0;

  func inc() {
    n = n + 1;
    return n;
  }
}

module Shadow {
  var x = 1;

  func locally() {
    var x = 2;
    return x;
  }

  func from_module() {
    return x;
  }
}

func __main__() {
  assert(Math.pi_approx == 3, "module-level global access");
  assert(Math.double(5) == 10, "module function call");
  assert(Math.quadruple(3) == 12, "intra-module function reference");

  assert(Outer.Mid.Inner.reach_top() == 42, "nested module reaches outer");
  assert(Outer.Mid.bump_top() == 43, "cross-module mutation: first bump");
  assert(Outer.top == 43, "mutation persisted on outer module");
  assert(Outer.Mid.bump_top() == 44, "second bump increments again");

  assert(Parity.is_even(6) == 1, "mutual recursion: even(6)");
  assert(Parity.is_odd(7) == 1, "mutual recursion: odd(7)");
  assert(Parity.is_even(7) == 0, "mutual recursion: even(7)");

  assert(Counter.inc() == 1, "counter first inc");
  assert(Counter.inc() == 2, "counter second inc");
  assert(Counter.inc() == 3, "counter third inc");

  assert(Shadow.locally() == 2, "local var shadows module global");
  assert(Shadow.from_module() == 1, "module global without local shadow");
}
