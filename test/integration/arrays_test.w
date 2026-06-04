func sum_array(arr) {
  var total = 0;
  var i = 0;
  var n = len(arr);
  while (i < n) {
    total = total + arr[i];
    i = i + 1;
  }
  return total;
}

func __main__() {
  var empty = new [];
  assert(len(empty) == 0, "empty array has length 0");

  var triple = new [10, 20, 30];
  assert(len(triple) == 3, "literal array length");
  assert(triple[0] == 10, "index 0");
  assert(triple[1] == 20, "index 1");
  assert(triple[2] == 30, "index 2");

  triple[1] = 200;
  assert(triple[1] == 200, "indexed assignment mutates element");
  assert(triple[0] == 10, "neighbor element untouched after write");

  assert(sum_array(new [1, 2, 3, 4, 5]) == 15, "while-loop sum over array");

  var row0 = new [1, 2];
  var row1 = new [3, 4, 5];
  var nested = new [row0, row1];
  assert(len(nested) == 2, "outer length");
  assert(len(nested[0]) == 2, "inner[0] length");
  assert(len(nested[1]) == 3, "inner[1] length");
  assert(nested[1][2] == 5, "nested indexing reads the right value");

  nested[0][0] = 99;
  assert(nested[0][0] == 99, "nested indexed assignment mutates inner array");
  assert(row0[0] == 99, "outer array stores reference, mutation visible via row0");

  var first_three = new [1, 2, 3];
  var expr_elems = new [1 + 2, 3 * 4, sum_array(first_three)];
  assert(expr_elems[0] == 3, "expression literal at index 0");
  assert(expr_elems[1] == 12, "expression literal at index 1");
  assert(expr_elems[2] == 6, "call expression literal at index 2");
}
