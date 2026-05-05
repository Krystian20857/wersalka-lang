## Language sketch

### Simple example

```
# sample.w

# Hello world
print("Hi!\n")

# Expressions/statements
var result = 2 * (2 + 2);
print("2 * (2 + 2) = {result}\n")
s
var a = readln();
var b = readln();
if (a > b) {
    print("a is greater than b");
} else {
    print("a is less than b");
}

var arr = ["a", "b", "c"]      # create array

# only some types can be placed in `for`
for (elem in arr) {
}

for (n in range(0, 10)) {
    print("n = {cast(n, str)}");
}

# casts, types
# normal cast
var str = cast(1, str);         # str - builtin symbol
var number = cast("123", int)   # int - builtin symbol


# normal string template
var str = "hello, 2 + 2 = {2 + 2}";

# multi-line string template
var a = 2
var b = 2
var str = """
          hello, from multi line string
          a = b + c
          a = {a}
          b = {b}
          a = { a + b }
          """;
          
# objects
func hello() {
    print("Hello!\n");
}

var obj = {};
obj.property = "hello";
obj.function = hello;


```
