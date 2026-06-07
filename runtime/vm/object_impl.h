//
// Created by nothingbutyou on 5/30/26.
//

#ifndef WERSALKALANG_OBJECT_IMPL_H
#define WERSALKALANG_OBJECT_IMPL_H

#include "runtime/vm/code_object.h"
#include "runtime/vm/value.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

class ShapedObject;
class ShapeTree;
class ClosureContextObject;

class StringObject : public Object {
 public:
  static constexpr auto kKind = ObjectKind::kString;
  static constexpr int kCharsOffset = kHeaderSize + sizeof(int);

  int length() const { return length_; }
  char* GetCharsPtr() { return reinterpret_cast<char*>(this) + kCharsOffset; }
  char* Begin() { return GetCharsPtr(); }
  char* End() { return GetCharsPtr() + length(); }

  std::span<const char> GetChars() const {
    return std::span(reinterpret_cast<const char*>(this) + kCharsOffset,
                     length());
  }
  std::string_view ToStringView() const { return std::string_view{GetChars()}; }

  static int SizeFor(const int length) { return length + kCharsOffset; }

  static GCPtr<StringObject> New(GC* gc, std::string_view str);
  static GCPtr<StringObject> Concat(GC* gc, GCPtr<StringObject> left,
                                    GCPtr<StringObject> right);

  bool operator==(const StringObject& other) const {
    return this->ToStringView() == other.ToStringView();
  }

  auto operator<=>(const StringObject& other) const {
    return this->ToStringView() <=> other.ToStringView();
  }

  operator std::string_view() const { return ToStringView(); }

 private:
  explicit StringObject(const int length)
      : Object(ObjectKind::kString), length_(length) {}

  friend class GC;

  int length_;
};

struct ModuleAlias {
  std::string_view name;
  ShapedObject* module;
};

class FunctionObject : public Object {
 public:
  static constexpr auto kKind = ObjectKind::kFunction;

  ZoneStr name() const { return name_; }
  ZonePtr<CodeObject> code_obj() const { return code_obj_; }
  Tagged<ShapedObject> module() const { return module_; }
  std::span<const ModuleAlias> ancestor_aliases() const {
    return ancestor_aliases_;
  }

 private:
  explicit FunctionObject(const ZoneStr name,
                          const ZonePtr<CodeObject> code_obj,
                          const Tagged<ShapedObject> module =
                              Tagged<ShapedObject>(Value::CreateNull()),
                          const std::span<const ModuleAlias> aliases = {})
      : Object(ObjectKind::kFunction),
        name_(name),
        code_obj_(code_obj),
        module_(module),
        ancestor_aliases_(aliases) {}

  friend class GC;
  friend class GCVisitor;

  ZoneStr name_;
  ZonePtr<CodeObject> code_obj_;
  Tagged<ShapedObject> module_;
  std::span<const ModuleAlias> ancestor_aliases_;
};

class ArrayObject : public Object {
 public:
  static constexpr auto kKind = ObjectKind::kArray;

  static GCPtr<ArrayObject> New(GC* gc, std::span<Value> elements);
  static GCPtr<ArrayObject> New(GC* gc, int size);
  static GCPtr<ArrayObject> NewStringArray(
      GC* gc, std::span<const std::string_view> strings);
  static GCPtr<ArrayObject> Concat(GC* gc, GCPtr<ArrayObject> left,
                                   GCPtr<ArrayObject> right);
  static GCPtr<ArrayObject> Add(GC* gc, GCPtr<ArrayObject> left, Value value);

  int length() const { return length_; }
  std::span<Value> GetElements() {
    return std::span(GetElementsPtr(), length());
  }

 private:
  static constexpr auto kLengthOffset = kHeaderSize;
  static constexpr auto kValuesOffset = kLengthOffset + sizeof(int);

  explicit ArrayObject(const int length)
      : Object(ObjectKind::kArray), length_(length) {}

  friend class GC;

  Value* GetElementsPtr() {
    return reinterpret_cast<Value*>(reinterpret_cast<char*>(this) +
                                    kValuesOffset);
  }

  int length_;
};

class Shape : public Object {
 public:
  static constexpr auto kKind = ObjectKind::kShape;

  Tagged<Shape> parent() const { return parent_; }
  Tagged<StringObject> field_name() const { return field_name_; }
  int slot_index() const { return slot_index_; }
  int transition_count() const {
    return transitions_.IsNull() ? 0 : transitions_->size_;
  }

 private:
  friend class ShapeTree;
  friend class GC;
  friend class GCVisitor;

  struct Transition {
    Tagged<StringObject> name;
    Tagged<Shape> child;
  };
  static_assert(std::is_trivially_destructible_v<Transition>);

  class TransitionArray : public Object {
   public:
    static constexpr auto kKind = ObjectKind::kTransitionArray;
    static GCPtr<TransitionArray> New(GC* gc, int size);

    std::span<Transition> Get() { return std::span(GetPtr(), size_); }

   private:
    friend class GC;
    friend class GCVisitor;
    friend class Shape;
    friend class ShapeTree;

    explicit TransitionArray(const int size)
        : Object(ObjectKind::kTransitionArray), size_(size) {}

    static int SizeFor(int size) {
      return sizeof(TransitionArray) + size * sizeof(Transition);
    }
    Transition* GetPtr() {
      return reinterpret_cast<Transition*>(reinterpret_cast<char*>(this) +
                                           sizeof(TransitionArray));
    }

    int size_;
  };

  explicit Shape(const Tagged<Shape> parent,
                 const Tagged<StringObject> field_name, const int slot_index)
      : Object(ObjectKind::kShape),
        parent_(parent),
        field_name_(field_name),
        slot_index_(slot_index) {}

  static GCPtr<Shape> New(GC* gc, Tagged<Shape> parent,
                          Tagged<StringObject> field_name, int slot_index);

  Tagged<Shape> parent_;
  Tagged<StringObject> field_name_;
  int slot_index_;
  Tagged<TransitionArray> transitions_;
};

class ShapeTree {
 public:
  explicit ShapeTree(GC* gc);

  GCPtr<Shape> ShapeOf(Tagged<ArrayObject> field_names) const;
  GCPtr<Shape> TransitionOf(Tagged<Shape> shape,
                            Tagged<StringObject> field) const;

  // slow offset resolve path
  int OffsetOf(Tagged<Shape> shape, std::string_view field) const;

  Tagged<Shape> root() const { return root_; }

 private:
  friend class GCVisitor;

  GC* gc_;
  Tagged<Shape> root_;
};

class ShapedObject : public Object {
 public:
  static constexpr auto kKind = ObjectKind::kShapedObject;

  Tagged<Shape> shape() const { return shape_; }

  std::span<Value> GetFields() { return values_.Get()->GetSlots(); }

  void TransitionTo(GC* gc, Tagged<Shape> new_shape);

  static GCPtr<ShapedObject> New(GC* gc, Tagged<Shape> shape);

 private:
  class ValueArray : public Object {
   public:
    static constexpr auto kKind = ObjectKind::kValueArray;

    std::span<Value> GetSlots() { return std::span(GetSlotsPtr(), capacity_); }
    int capacity() const { return capacity_; }

    static GCPtr<ValueArray> New(GC* gc, int capacity);
    static GCPtr<ValueArray> Grow(GC* gc, GCPtr<ValueArray> old,
                                  int new_capacity);

   private:
    static int SizeFor(int capacity) {
      return sizeof(ValueArray) + capacity * sizeof(Value);
    }
    Value* GetSlotsPtr() {
      return reinterpret_cast<Value*>(reinterpret_cast<char*>(this) +
                                      sizeof(ValueArray));
    }
    explicit ValueArray(const int capacity)
        : Object(ObjectKind::kValueArray), capacity_(capacity) {}

    friend class GC;
    friend class GCVisitor;
    friend class ShapedObject;

    int capacity_;
  };

  explicit ShapedObject(const Tagged<Shape> shape, GCPtr<ValueArray> values)
      : Object(ObjectKind::kShapedObject), shape_(shape), values_(values) {}

  friend class GC;
  friend class GCVisitor;

  Tagged<Shape> shape_;
  Tagged<ValueArray> values_;
};

class ModuleMetaObject : public Object {
 public:
  static constexpr auto kKind = ObjectKind::kModuleMeta;

  Tagged<StringObject> name() const { return name_; }
  bool anonymous() const { return anonymous_; }

  static GCPtr<ModuleMetaObject> New(GC* gc, Tagged<StringObject> name,
                                     bool anonymous);

 private:
  explicit ModuleMetaObject(const Tagged<StringObject> name,
                            const bool anonymous)
      : Object(ObjectKind::kModuleMeta), name_(name), anonymous_(anonymous) {}

  friend class GC;
  friend class GCVisitor;

  Tagged<StringObject> name_;
  bool anonymous_;
};

class ClosureObject : public Object {
 public:
  static constexpr auto kKind = ObjectKind::kClosure;

  Tagged<FunctionObject> function() const { return function_; }
  Tagged<ClosureContextObject> outer_context() const { return outer_context_; }

  static GCPtr<ClosureObject> New(GC* gc, Tagged<FunctionObject> function,
                                  Tagged<ClosureContextObject> outer_context);

 private:
  explicit ClosureObject(const Tagged<FunctionObject> function,
                         const Tagged<ClosureContextObject> outer_context)
      : Object(ObjectKind::kClosure),
        function_(function),
        outer_context_(outer_context) {}

  friend class GC;
  friend class GCVisitor;

  Tagged<FunctionObject> function_;
  Tagged<ClosureContextObject> outer_context_;
};

class ClosureContextObject : public Object {
 public:
  static constexpr auto kKind = ObjectKind::kClosureContext;
  static constexpr auto kParentOffset = kHeaderSize;
  static constexpr auto kValueCountOffset =
      kParentOffset + sizeof(Tagged<ClosureContextObject>);
  static constexpr auto kValuesOffset = kValueCountOffset + sizeof(int);

  static GCPtr<ClosureContextObject> New(GC* gc,
                                         Tagged<ClosureContextObject> parent,
                                         int value_count);

  std::span<Value> GetValues() {
    return std::span(GetValuesPtr(), value_count_);
  }

  Tagged<ClosureContextObject> parent() const { return parent_; }
  void set_parent(Tagged<ClosureContextObject> parent) { parent_ = parent; }

 private:
  explicit ClosureContextObject(const Tagged<ClosureContextObject> parent,
                                const int value_count)
      : Object(ObjectKind::kClosureContext),
        parent_(parent),
        value_count_(value_count) {}

  friend class GC;
  friend class GCVisitor;

  Value* GetValuesPtr() {
    return reinterpret_cast<Value*>(reinterpret_cast<char*>(this) +
                                    kValuesOffset);
  }

  Tagged<ClosureContextObject> parent_;  // can be nullptr
  int value_count_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_OBJECT_IMPL_H
