//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_VALUE_H
#define WERSALKALANG_VALUE_H

#include <cstdint>

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

  constexpr static Raw kNullPayload = 0b00000;
  constexpr static Raw kTruePayload = 0b11000;
  constexpr static Raw kFalsePayload = 0b01000;
};

class Value {
 public:
  // layout LSB -> MSB:
  //  - 3 tag bits, ValueTag
  using Raw = uint64_t;

  Value() : Value(ValueTags::kSpecialTag | ValueTags::kNullPayload) {}

  static Value CreateInt(const int64_t value) {
    CHECK(value >= ValueTags::kIntMinValue && value <= ValueTags::kIntMaxValue);
    return Value{(value << 3) | ValueTags::kIntTag};
  }

  static Value CreateBool(const bool value) {
    if (value) {
      return Value{ValueTags::kSpecialTag | ValueTags::kTruePayload};
    } else {
      return Value{ValueTags::kSpecialTag | ValueTags::kFalsePayload};
    }
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

  static Value CreateNull() { return Value{}; }

  Raw GetRawValue() const { return bits_; }
  Raw GetTagValue() const { return bits_ & ValueTags::kTagMask; }
  int64_t GetIntValue() const {
    CHECK(IsInt());
    return bits_ >> 3;
  }
  bool GetBoolValue() const {
    CHECK(IsBool());
    return bits_ == (ValueTags::kSpecialTag | ValueTags::kTruePayload);
  }

  bool IsInt() const { return GetTagValue() == ValueTags::kIntTag; }
  bool IsObject() const { return GetTagValue() == ValueTags::kObjectTag; }
  bool IsNull() const {
    return bits_ == (ValueTags::kSpecialTag | ValueTags::kNullPayload);
  }
  bool IsBool() const {
    return bits_ == (ValueTags::kSpecialTag | ValueTags::kTruePayload) ||
           bits_ == (ValueTags::kSpecialTag | ValueTags::kFalsePayload);
  }

  Object* GetObject() const {
    CHECK(IsObject());
    return reinterpret_cast<Object*>(bits_ & ~(ValueTags::kTagMask));
  }

  template <typename T>
  std::optional<T*> GetCheckedObject() const {
    static_assert(std::is_base_of_v<Object, T> || std::is_same_v<Object, T>,
                  "T must derive from `Object`");
    CHECK(IsObject());
    Object* object = GetObject();
    if (object->kind() == T::kKind) {
      return static_cast<T*>(object);
    } else {
      return std::nullopt;
    }
  }

 private:
  constexpr explicit Value(const uint64_t bits) : bits_(bits) {}

  Raw bits_;
};

static_assert(sizeof(Value) == 8);
static_assert(std::is_trivially_copyable_v<Value>);

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_VALUE_H
