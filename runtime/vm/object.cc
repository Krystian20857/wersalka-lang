//
// Created by nothingbutyou on 5/17/26.
//

#include "object.h"

#include <algorithm>
#include <memory>

namespace wersalka {
namespace lang {
namespace runtime {
GCPtr<StringObject> StringObject::New(GC* gc, std::string_view str) {
  const auto string_object = gc->NewSized<StringObject>(SizeFor(str.size()), str.size());
  std::uninitialized_copy(str.begin(), str.end(), string_object->GetChars());
  return string_object;
}
GCPtr<StringObject> StringObject::Concat(GC* gc, GCPtr<StringObject> left,
                                         GCPtr<StringObject> right) {
  const auto final_size = left->length() + right->length();
  const auto string_object = gc->NewSized<StringObject>(SizeFor(final_size), final_size);
  std::uninitialized_copy(left->Begin(), left->End(),
                          string_object->GetChars());
  std::uninitialized_copy(right->Begin(), right->End(),
                          string_object->GetChars() + left->length());
  return string_object;
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
