//
// Created by nothingbutyou on 5/31/26.
//

#ifndef WERSALKALANG_BUILTINS_H
#define WERSALKALANG_BUILTINS_H

#include "absl/container/flat_hash_map.h"
#include "runtime/gc/gc.h"
#include "runtime/vm/object_impl.h"

namespace wersalka {
namespace lang {
namespace runtime {

class Builtins;
class Runtime;

class FixedShape {
 public:
  struct Field {
    std::string_view name;
    Value value;
  };

  explicit FixedShape(Runtime* runtime, const GCPtr<Shape> shape)
      : runtime_(runtime), shape_(shape) {}

  GCPtr<ShapedObject> New() const;
  GCPtr<ShapedObject> New(std::initializer_list<Field> fields) const;
  void Set(GCPtr<ShapedObject> object, std::string_view field,
           Value value) const;
  Value Get(GCPtr<ShapedObject> object, std::string_view field) const;

  GCPtr<Shape> shape() const { return shape_; }

 private:
  friend class Builtins;

  Runtime* runtime_;
  GCPtr<Shape> shape_;
  absl::flat_hash_map<std::string_view, int> offset_cache_;
};

class Builtins {
  // Declared first so it is initialized before the FixedShape members below.
  Runtime* runtime_;

 public:
  explicit Builtins(Runtime* runtime) : runtime_(runtime) {}

  void RegisterBuiltIns() const;

  FixedShape Shape_GCStats =
      CreateFixedShape({"allocated_bytes", "alive_bytes"});
  FixedShape Shape_Exception =
      CreateFixedShape({"message", "cause", "stacktrace"});
  FixedShape Shape_Frame =
      CreateFixedShape({"function_name", "line", "bytecode_index"});

 private:
  FixedShape CreateFixedShape(
      std::initializer_list<std::string_view> fields) const;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_BUILTINS_H
