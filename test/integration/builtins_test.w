func __main__() {
  assert(cast(42, "string") == "42", "int -> string cast renders decimal");
  assert(cast("17", "int") == null, "non-numeric string -> int is not supported yet");
  assert(cast(true, "int") == 1, "bool true -> int");
  assert(cast(false, "int") == 0, "bool false -> int");
  assert(cast(null, "int") == 0, "null -> int yields 0");

  assert(cast(0, "bool") == false, "int 0 -> bool false");
  assert(cast(1, "bool") == true, "int 1 -> bool true");
  assert(cast(null, "bool") == false, "null -> bool false");

  var person = new {};
  person.name = "Ada";
  person.age = 16;
  assert(person.name == "Ada", "shaped object field read after write");
  assert(person.age == 16, "shaped object stores ints");

  person.age = person.age + 1;
  assert(person.age == 17, "shaped object field mutated in place");

  person.role = "engineer";
  assert(person.role == "engineer", "shaped object grows shape on new field");
  assert(person.name == "Ada", "growing shape preserves prior fields");

  var missing = person.unknown_field;
  assert(missing == null, "missing field reads as null");

  var sentence = "name=" + person.name + ", age=" + person.age;
  assert(sentence == "name=Ada, age=17", "string concat with mixed types");
}
