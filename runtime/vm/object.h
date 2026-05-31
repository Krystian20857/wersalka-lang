//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_OBJECT_H
#define WERSALKALANG_OBJECT_H

#include <string_view>

#include "runtime/gc/gc.h"

namespace wersalka {
namespace lang {
namespace runtime {

enum class ObjectKind {
  kFunction,
  kNativeFunction,
  kBigInt,
  kString,
  kShape,
  kTransitionArray,
  kShapedObject,  // `instance` of shape
  kValueArray,    // private backing store for ShapedObject
  kArray
};

class Object {
 public:
  static constexpr int kHeaderSize = sizeof(ObjectKind);

  ObjectKind kind() const { return kind_; }

 protected:
  explicit Object(const ObjectKind kind) : kind_(kind) {}

 private:
  ObjectKind kind_;

  template <typename T, typename... Args>
  friend GCPtr<T> GC::New(Args&&... args);
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_OBJECT_H
