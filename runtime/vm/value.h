//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_VALUE_H
#define WERSALKALANG_VALUE_H

#include "absl/log/check.h"
#include "runtime/vm/object.h"

namespace wersalka {
namespace lang {
namespace runtime {

struct ValueTags {
  using Raw = uint64_t;

  constexpr static int64_t kIntMinValue = -(int64_t{1} << 60);
  constexpr static int64_t kIntMaxValue = (int64_t{1} << 60) - 1;

  constexpr static Raw kTagMask = 0b111;
  constexpr static Raw kIntTag = 0b000;
  constexpr static Raw kObjectTag = 0b001;
  constexpr static Raw kSpecialTag = 0b010;  // true, false, null, etc.
  constexpr static Raw kFloatTag = 0b011;

  constexpr static Raw kNullPayload = 0b00000;
  constexpr static Raw kTruePayload = 0b11000;
  constexpr static Raw kFalsePayload = 0b01000;
};

class Value {
 public:
  // layout LSB -> MSB:
  //  - 3 tag bits, ValueTag
  using Raw = uint64_t;

  constexpr Value() : Value(ValueTags::kSpecialTag | ValueTags::kNullPayload) {}

  static constexpr Value CreateInt(const int64_t value) {
    CHECK(value >= ValueTags::kIntMinValue && value <= ValueTags::kIntMaxValue);
    return Value{(value << 3) | ValueTags::kIntTag};
  }

  static constexpr Value CreateBool(const bool value) {
    if (value) {
      return Value{ValueTags::kSpecialTag | ValueTags::kTruePayload};
    } else {
      return Value{ValueTags::kSpecialTag | ValueTags::kFalsePayload};
    }
  }

  static constexpr Value CreateFloat(const float value) {
    const auto bits = std::bit_cast<uint32_t, float>(value);
    return Value{ValueTags::kFloatTag | (static_cast<uint64_t>(bits) << 3)};
  }

  // TODO: maybe use concept here
  template <typename T>
  static Value CreateObject(T* obj) {
    static_assert(std::is_base_of_v<Object, T> || std::is_same_v<Object, T>,
                  "T must derive from `Object`");
    Object* base = obj;
    const auto ptr = reinterpret_cast<uintptr_t>(base);
    // ptr have to be 8-bit aligned
    CHECK((ptr & ValueTags::kTagMask) == 0);
    return Value{ptr | ValueTags::kObjectTag};
  }

  static constexpr Value CreateNull() { return Value{}; }

  constexpr Raw GetRawValue() const { return bits_; }
  constexpr Raw GetTagValue() const { return bits_ & ValueTags::kTagMask; }
  constexpr int64_t GetIntValue() const {
    CHECK(IsInt());
    return static_cast<int64_t>(bits_) >> 3;
  }
  constexpr bool GetBoolValue() const {
    CHECK(IsBool());
    return bits_ == (ValueTags::kSpecialTag | ValueTags::kTruePayload);
  }
  constexpr float GetFloatValue() const {
    CHECK(IsFloat());
    return std::bit_cast<float, uint32_t>(static_cast<uint32_t>(bits_ >> 3));
  }

  constexpr bool IsInt() const { return GetTagValue() == ValueTags::kIntTag; }
  constexpr bool IsObject() const {
    return GetTagValue() == ValueTags::kObjectTag;
  }
  constexpr bool IsNull() const {
    return bits_ == (ValueTags::kSpecialTag | ValueTags::kNullPayload);
  }
  constexpr bool IsBool() const {
    return bits_ == (ValueTags::kSpecialTag | ValueTags::kTruePayload) ||
           bits_ == (ValueTags::kSpecialTag | ValueTags::kFalsePayload);
  }
  constexpr bool IsFloat() const {
    return GetTagValue() == ValueTags::kFloatTag;
  }

  GCPtr<Object> GetObject() const {
    CHECK(IsObject());
    return reinterpret_cast<Object*>(bits_ & ~(ValueTags::kTagMask));
  }

  template <typename T>
  std::optional<GCPtr<T>> GetObject() const;

  template <typename T>
  GCPtr<T> GetObjectUnchecked() const;

  constexpr bool IsObject(const ObjectKind kind) const {
    return IsObject() && GetObject()->kind() == kind;
  }

 private:
  constexpr explicit Value(const uint64_t bits) : bits_(bits) {}

  Raw bits_;
};

template <typename T>
std::optional<GCPtr<T>> Value::GetObject() const {
  static_assert(std::is_base_of_v<Object, T> || std::is_same_v<Object, T>,
                "T must derive from `Object`");
  CHECK(IsObject());
  Object* object = GetObject();
  if (object->kind() == T::kKind) {
    return static_cast<GCPtr<T>>(object);
  } else {
    return std::nullopt;
  }
}
template <typename T>
GCPtr<T> Value::GetObjectUnchecked() const {
  static_assert(std::is_base_of_v<Object, T> || std::is_same_v<Object, T>,
                "T must derive from `Object`");
  CHECK(IsObject());
  Object* object = GetObject();
  CHECK(object->kind() == T::kKind);
  return static_cast<GCPtr<T>>(object);
}

static_assert(sizeof(Value) == 8);
static_assert(std::is_trivially_copyable_v<Value>);

template <typename T>
class Handle {
 public:
  friend class GC;
  friend class GCVisitor;
  friend class Value;

  GCPtr<T> GetPtr() const { return value_->GetObject(); }
  void SetPtr(const Value value) const { *value_ = value; }

 private:
  explicit Handle(Value* value) : value_(value) {}

  Value* value_;
};

// type-safe Object<...> value wrapper
template <typename T>
class Tagged {
 public:
  Tagged() : value_(Value::CreateNull()) {}
  Tagged(Value value) : value_(value) {
    CHECK((value.IsObject() && value.GetObject()->kind() == T::kKind) ||
          value.IsNull());
  }
  // static_assert(std::is_same_v<Object, T> || std::is_base_of_v<Object, T>);
  Tagged(T* ptr) : value_(Value::CreateObject(ptr)) {}

  T* Get() const { return static_cast<T*>(value_.GetObject()); }
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }
  bool IsNull() const { return value_.IsNull(); }

  Value& AsValue() { return value_; }

 private:
  Value value_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_VALUE_H
