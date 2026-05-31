//
// Created by nothingbutyou on 5/30/26.
//

#include "runtime/vm/object_impl.h"

namespace wersalka {
namespace lang {
namespace runtime {
GCPtr<StringObject> StringObject::New(GC* gc, const std::string_view str) {
  const auto string_object =
      gc->NewSized<StringObject>(SizeFor(str.size()), str.size());
  std::uninitialized_copy(str.begin(), str.end(), string_object->GetCharsPtr());
  return string_object;
}
GCPtr<StringObject> StringObject::Concat(GC* gc, const GCPtr<StringObject> left,
                                         const GCPtr<StringObject> right) {
  const auto final_size = left->length() + right->length();
  const auto string_object =
      gc->NewSized<StringObject>(SizeFor(final_size), final_size);
  std::uninitialized_copy(left->Begin(), left->End(),
                          string_object->GetCharsPtr());
  std::uninitialized_copy(right->Begin(), right->End(),
                          string_object->GetCharsPtr() + left->length());
  return string_object;
}
GCPtr<ArrayObject> ArrayObject::New(GC* gc, const std::span<Value> elements) {
  const auto array_object = New(gc, elements.size());
  std::uninitialized_copy(elements.begin(), elements.end(),
                          array_object->GetElementsPtr());
  return array_object;
}
GCPtr<ArrayObject> ArrayObject::New(GC* gc, int size) {
  const auto array_object = gc->NewSized<ArrayObject>(
      sizeof(ArrayObject) + size * sizeof(Value), size);
  return array_object;
}
GCPtr<ArrayObject> ArrayObject::NewStringArray(
    GC* gc, const std::span<const std::string_view> strings) {
  const auto array = New(gc, strings.size());
  for (auto n = 0; n < strings.size(); n++) {
    array->GetElements()[n] =
        Value::CreateObject(StringObject::New(gc, strings[n]));
  }
  return array;
}
GCPtr<ArrayObject> ArrayObject::Concat(GC* gc, const GCPtr<ArrayObject> left,
                                       const GCPtr<ArrayObject> right) {
  // TODO: deffer actual concat, array can be just binary tree of concats
  const auto result_array = New(gc, left->length() + right->length());
  const auto left_elements = left->GetElements();
  const auto right_elements = right->GetElements();
  std::uninitialized_copy(left_elements.begin(), left_elements.end(),
                          result_array->GetElements().begin());
  std::uninitialized_copy(
      right_elements.begin(), right_elements.end(),
      result_array->GetElements().begin() + left_elements.size());
  return result_array;
}
GCPtr<ArrayObject> ArrayObject::Add(GC* gc, GCPtr<ArrayObject> left,
                                    Value value) {
  // TODO: deffer actual concat, array can be just binary tree of concats
  const auto result_array = New(gc, left->length() + 1);
  const auto left_elements = left->GetElements();
  std::uninitialized_copy(left_elements.begin(), left_elements.end(),
                          result_array->GetElements().begin());
  result_array->GetElements()[left->length()] = value;
  return result_array;
}

GCPtr<Shape> Shape::New(GC* gc, const Tagged<Shape> parent,
                        const Tagged<StringObject> field_name,
                        const int slot_index) {
  return gc->New<Shape>(parent, field_name, slot_index);
}
GCPtr<Shape::TransitionArray> Shape::TransitionArray::New(GC* gc,
                                                          const int size) {
  return gc->NewSized<TransitionArray>(SizeFor(size), size);
}
ShapeTree::ShapeTree(GC* gc) : gc_(gc) {
  root_ = Shape::New(gc, Value::CreateNull(), StringObject::New(gc, ""),
                     -1 /* first field shall have slot 0 */);
}
GCPtr<Shape> ShapeTree::ShapeOf(Tagged<ArrayObject> field_names) const {
  // TODO: safe tagged heap array
  for (auto value : field_names->GetElements()) {
    CHECK(value.IsObject() && value.GetObject()->kind() == ObjectKind::kString);
  }
  if (field_names->length() == 0) {
    return root_.Get();
  }
  GCPtr<Shape> shape = root_.Get();
  for (const auto element : field_names->GetElements()) {
    shape = TransitionOf(Tagged(shape), element);
  }
  return shape;
}
GCPtr<Shape> ShapeTree::TransitionOf(const Tagged<Shape> shape,
                                     const Tagged<StringObject> field) const {
  CHECK(field->length() > 0);

  // TODO: bsearch
  if (!shape->transitions_.IsNull()) {
    for (const auto& transition : shape->transitions_->Get()) {
      if (*transition.name == *field) {
        return transition.child.Get();
      }
    }
  }

  const int old_count = shape->transition_count();
  const auto new_shape =
      Shape::New(gc_, Tagged(shape.Get()), field, shape->slot_index() + 1);
  const auto new_array = Shape::TransitionArray::New(gc_, old_count + 1);
  if (old_count > 0) {
    const auto old = shape->transitions_->Get();
    std::uninitialized_copy(old.begin(), old.end(), new_array->GetPtr());
  }
  new_array->GetPtr()[old_count] = Shape::Transition{field, Tagged(new_shape)};
  std::ranges::sort(new_array->Get(), [](const auto& a, const auto& b) {
    return *a.name < *b.name;
  });
  shape->transitions_ = Tagged(new_array);

  return new_shape;
}
int ShapeTree::OffsetOf(const Tagged<Shape> shape,
                        const std::string_view field) const {
  auto current = shape.Get();
  while (current != nullptr) {
    if (*current->field_name_ == field) {
      return current->slot_index();
    }
    if (current->parent_.IsNull() || current == root_.Get()) {
      break;
    }
    current = current->parent_.Get();
  }
  return -1;
}
GCPtr<ShapedObject::ValueArray> ShapedObject::ValueArray::New(GC* gc,
                                                              int capacity) {
  const auto arr = gc->NewSized<ValueArray>(SizeFor(capacity), capacity);
  for (auto& slot : arr->GetSlots()) {
    slot = Value::CreateNull();
  }
  return arr;
}
GCPtr<ShapedObject::ValueArray> ShapedObject::ValueArray::Grow(
    GC* gc, GCPtr<ValueArray> old, int new_capacity) {
  const auto arr =
      gc->NewSized<ValueArray>(SizeFor(new_capacity), new_capacity);
  const auto old_slots = old->GetSlots();
  std::uninitialized_copy(old_slots.begin(), old_slots.end(),
                          arr->GetSlotsPtr());
  for (int i = old->capacity_; i < new_capacity; ++i) {
    arr->GetSlotsPtr()[i] = Value::CreateNull();
  }
  return arr;
}
GCPtr<ShapedObject> ShapedObject::New(GC* gc, Tagged<Shape> shape) {
  const int slot_count = std::max(shape->slot_index() + 1, 0);
  const auto values = ValueArray::New(gc, slot_count);
  return gc->New<ShapedObject>(shape, values);
}
void ShapedObject::TransitionTo(GC* gc, Tagged<Shape> new_shape) {
  if (shape_.Get() == new_shape.Get()) {
    return;
  }
  const int new_count = new_shape->slot_index() + 1;
  if (new_count > values_.Get()->capacity()) {
    values_ = Tagged(ValueArray::Grow(gc, values_.Get(), new_count));
  }
  shape_ = new_shape;
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
