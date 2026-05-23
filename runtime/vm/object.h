//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_OBJECT_H
#define WERSALKALANG_OBJECT_H

#include <string_view>

#include "runtime/vm/gc.h"

namespace wersalka {
namespace lang {
namespace runtime {

enum class ObjectKind {
  kFunction,
  kNativeFunction,
  kBigInt,
  kString,
  kShapedObject,
  kObjectArray
};

class Object {
 public:
  static constexpr int kHeaderSize = sizeof(ObjectKind);

  explicit Object(const ObjectKind kind) : kind_(kind) {}

  ObjectKind kind() const { return kind_; }

 private:
  ObjectKind kind_;
};

class StringObject : public Object {
 public:
  static constexpr int kCharsOffset = kHeaderSize + sizeof(int);

  explicit StringObject(const int length)
      : Object(ObjectKind::kString), length_(length), reserved_(0) {}

  int length() const { return length_; }
  char* GetChars() { return reinterpret_cast<char*>(this) + kCharsOffset; }
  char* Begin() { return GetChars(); }
  char* End() { return GetChars() + length(); }

  static int SizeFor(const int length) { return length + kCharsOffset; }

  static GCPtr<StringObject> New(GC* gc, std::string_view str);
  static GCPtr<StringObject> Concat(GC* gc, GCPtr<StringObject> left,
                                    GCPtr<StringObject> right);

 private:
  int length_;
  int reserved_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_OBJECT_H
