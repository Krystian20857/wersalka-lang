//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_OBJECT_H
#define WERSALKALANG_OBJECT_H

namespace wersalka {
namespace lang {
namespace runtime {

enum class ObjectKind { kFunction, kNativeFunction, kString, kHeapRef };

class Object {
 public:
  explicit Object(const ObjectKind kind) : kind_(kind) {}

  ObjectKind kind() const { return kind_; }

 private:
  ObjectKind kind_;
};

template <typename T>
struct ObjectTraits;

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_OBJECT_H
