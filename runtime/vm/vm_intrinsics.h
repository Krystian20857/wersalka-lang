//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_VM_INTRINSICS_H
#define WERSALKALANG_VM_INTRINSICS_H

#include <optional>
#include <string>
#include <string_view>

#include "absl/strings/str_format.h"
#include "runtime/gc/handle.h"
#include "runtime/vm/object_impl.h"
#include "runtime/vm/runtime.h"
#include "runtime/vm/value.h"

namespace wersalka {
namespace lang {
namespace runtime {

class VMThread;

class VMIntrinsics {
 public:
  static std::optional<int64_t> CoerceToInt(Value value);
  static std::optional<float> CoerceToFloat(Value value);
  static std::optional<int64_t> CoerceToBool(Value value);
  static GCPtr<StringObject> CoerceToString(Runtime* runtime, Value value);

  static bool IsTruthful(Value value);
  static Value Add(VMThread* thread, Value left, Value right);
  static Value Sub(VMThread* thread, Value left, Value right);
  static Value Negate(VMThread* thread, Value value);
  static std::string ToString(Runtime* runtime, Value value) {
    return ToString(runtime, value, 0);
  }
  static int IdentityHash(Runtime* runtime, const void* object);
  static Value Equals(VMThread* thread, Value left, Value right);

  template <typename Context>
  static void SetField(Context* context, Value object, Value field,
                       Value value);

  template <typename Context>
  static Value GetField(Context* context, Value object, Value field);

  static std::string_view GetValueTypeName(Value value);

  template <typename Op>
  static Value BinIntOp(VMThread* thread, Value left, Value right, Op op);

  template <typename Op>
  static Value BinFloatOp(VMThread* thread, Value left, Value right, Op op);

 private:
  static constexpr auto kMaxToStringDepth = 8;

  static Runtime* GetRuntime(const VMThread* thread);
  static Runtime* GetRuntime(const NativeContext* context) {
    return context->runtime;
  }

  static void ThrowException(VMThread* thread, Value value);
  static void ThrowException(const NativeContext* context, Value value) {
    *context->exception = value;
  }

  static std::string ToString(Runtime* runtime, Value value, int depth);
};

template <typename Op>
Value VMIntrinsics::BinIntOp(VMThread* thread, const Value left,
                             const Value right, Op op) {
  const auto left_coerced = CoerceToInt(left);
  const auto right_coerced = CoerceToInt(right);
  if (!left_coerced || !right_coerced) {
    ThrowException(thread,
                   GetRuntime(thread)->NewException(absl::StrFormat(
                       "Unsupported types for `int` binary operation, left: "
                       "`%s`, right: %s",
                       GetValueTypeName(left), GetValueTypeName(right))));
    return Value::CreateNull();
  }
  using Result = std::invoke_result_t<Op, int64_t, int64_t>;
  if constexpr (std::is_same_v<Result, bool>) {
    return Value::CreateBool(op(*left_coerced, *right_coerced));
  } else {
    return Value::CreateInt(op(*left_coerced, *right_coerced));
  }
}

template <typename Op>
Value VMIntrinsics::BinFloatOp(VMThread* thread, const Value left,
                               const Value right, Op op) {
  const auto left_coerced = CoerceToFloat(left);
  const auto right_coerced = CoerceToFloat(right);
  if (!left_coerced || !right_coerced) {
    ThrowException(thread,
                   GetRuntime(thread)->NewException(absl::StrFormat(
                       "Unsupported types for `float` binary operation, left: "
                       "`%s`, right: %s",
                       GetValueTypeName(left), GetValueTypeName(right))));
  }
  return Value::CreateFloat(op(*left_coerced, *right_coerced));
}

template <typename Context>
void VMIntrinsics::SetField(Context* context, const Value object,
                            const Value field, const Value value) {
  if (!object.IsObject() ||
      object.GetObject()->kind() != ObjectKind::kShapedObject) {
    ThrowException(context, GetRuntime(context)->NewException(absl::StrFormat(
                                "Invalid type, `object` required, got `%s`",
                                GetValueTypeName(object))));
    return;
  }
  if (!field.IsObject() || field.GetObject()->kind() != ObjectKind::kString) {
    ThrowException(context, GetRuntime(context)->NewException(
                                "Invalid type, `string` required"));
    return;
  }
  auto* runtime = GetRuntime(context);
  const auto shaped = object.GetObjectUnchecked<ShapedObject>();
  const auto field_str = field.GetObjectUnchecked<StringObject>();
  const auto new_shape =
      runtime->shaped_tree()->TransitionOf(shaped->shape(), Tagged(field_str));
  shaped->TransitionTo(runtime->gc(), new_shape);
  shaped->GetFields()[new_shape->slot_index()] = value;
}

template <typename Context>
Value VMIntrinsics::GetField(Context* context, const Value object,
                             const Value field) {
  if (!object.IsObject() ||
      object.GetObject()->kind() != ObjectKind::kShapedObject) {
    ThrowException(context, GetRuntime(context)->NewException(absl::StrFormat(
                                "Invalid type, `object` required, got `%s`",
                                GetValueTypeName(object))));
    return Value::CreateNull();
  }
  if (!field.IsObject() || field.GetObject()->kind() != ObjectKind::kString) {
    ThrowException(context, GetRuntime(context)->NewException(
                                "Invalid type, `string` required"));
    return Value::CreateNull();
  }
  auto* runtime = GetRuntime(context);
  const auto shaped = object.GetObjectUnchecked<ShapedObject>();
  const auto field_str = field.GetObjectUnchecked<StringObject>();
  const int slot =
      runtime->shaped_tree()->OffsetOf(shaped->shape(), *field_str);
  return slot < 0 ? Value::CreateNull() : shaped->GetFields()[slot];
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_VM_INTRINSICS_H
