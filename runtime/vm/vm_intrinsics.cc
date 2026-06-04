//
// Created by nothingbutyou on 5/17/26.
//

#include "vm_intrinsics.h"

#include <algorithm>
#include <vector>

#include "vm.h"

namespace wersalka {
namespace lang {
namespace runtime {

Runtime* VMIntrinsics::GetRuntime(const VMThread* thread) {
  return thread->runtime();
}

void VMIntrinsics::ThrowException(VMThread* thread, Value value) {
  thread->ThrowException(value);
}

std::string_view VMIntrinsics::GetValueTypeName(Value value) {
  if (value.IsInt()) {
    return "int";
  }
  if (value.IsFloat()) {
    return "float";
  }
  if (value.IsBool()) {
    return "bool";
  }
  if (value.IsNull()) {
    return "null";
  }
  if (value.IsObject()) {
    switch (value.GetObject()->kind()) {
      case ObjectKind::kFunction:
        return "function";
      case ObjectKind::kNativeFunction:
        return "native_function";
      case ObjectKind::kBigInt:
        return "big_int";
      case ObjectKind::kString:
        return "string";
      case ObjectKind::kShape:
        return "object_shape";
      case ObjectKind::kTransitionArray:
        return "object_shape_transition_array";
      case ObjectKind::kShapedObject:
        return "object";
      case ObjectKind::kValueArray:
        return "object_field_array";
      case ObjectKind::kArray:
        return "array";
      case ObjectKind::kModuleMeta:
        return "module";
    }
  }
  ABSL_UNREACHABLE();
}

std::optional<int64_t> VMIntrinsics::CoerceToInt(Value value) {
  // TODO: string -> int parse
  if (value.IsInt()) {
    return value.GetIntValue();
  }
  if (value.IsBool()) {
    return value.GetBoolValue() ? 1 : 0;
  }
  if (value.IsNull()) {
    return 0;
  }
  return std::nullopt;
}

std::optional<float> VMIntrinsics::CoerceToFloat(Value value) {
  // TODO: string -> float parse
  if (value.IsFloat()) {
    return value.GetFloatValue();
  }
  if (value.IsInt()) {
    return static_cast<float>(value.GetIntValue());
  }
  return CoerceToInt(value).transform(
      [](const auto it) { return static_cast<float>(it); });
}

std::optional<int64_t> VMIntrinsics::CoerceToBool(Value value) {
  // TODO: string -> bool parse
  if (value.IsBool()) {
    return value.GetBoolValue();
  }
  if (value.IsInt()) {
    return value.GetIntValue() > 0;
  }
  if (value.IsNull()) {
    return false;
  }
  return std::nullopt;
}

GCPtr<StringObject> VMIntrinsics::CoerceToString(Runtime* runtime,
                                                 const Value value) {
  return StringObject::New(runtime->gc(), ToString(runtime, value));
}

bool VMIntrinsics::IsTruthful(Value value) {
  if (value.IsNull()) {
    return false;
  }
  if (value.IsBool()) {
    return value.GetBoolValue();
  }
  if (value.IsInt()) {
    return value.GetIntValue() > 0;
  }
  return false;
}

Value VMIntrinsics::Add(VMThread* thread, Value left, Value right) {
  // string concat
  const auto do_concat =
      left.IsObject(ObjectKind::kString) || right.IsObject(ObjectKind::kString);
  if (do_concat) {
    HandleScope scope(thread);
    const auto left_string =
        scope.Alloc(CoerceToString(thread->runtime(), left));
    const auto right_string =
        scope.Alloc(CoerceToString(thread->runtime(), right));
    return Value::CreateObject(StringObject::Concat(thread->runtime()->gc(),
                                                    left_string, right_string));
  }
  if (right.IsFloat() || left.IsFloat()) {
    return BinFloatOp(thread, left, right,
                      [](auto a, auto b) { return a + b; });
  } else {
    return BinIntOp(thread, left, right, [](auto a, auto b) { return a + b; });
  }
}

Value VMIntrinsics::Sub(VMThread* thread, Value left, Value right) {
  if (right.IsFloat() || left.IsFloat()) {
    return BinFloatOp(thread, left, right,
                      [](auto a, auto b) { return a - b; });
  } else {
    return BinIntOp(thread, left, right, [](auto a, auto b) { return a - b; });
  }
}

Value VMIntrinsics::Negate(VMThread* thread, Value value) {
  const auto value_coerced = CoerceToInt(value);
  if (!value_coerced) {
    thread->ThrowException(thread->runtime()->NewException(
        "Cannot negate, invalid type, `int` type required"));
    return Value::CreateNull();
  }
  return Value::CreateInt(-(*value_coerced));
}

std::string VMIntrinsics::ToString(Runtime* runtime, Value value, int depth) {
  // TODO: remove unnecessary string intermediate allocation here
  if (depth >= kMaxToStringDepth) {
    return "[...]";
  }
  if (value.IsNull()) {
    return "null";
  }
  if (value.IsInt()) {
    return std::to_string(value.GetIntValue());
  }
  if (value.IsBool()) {
    return value.GetBoolValue() ? "true" : "false";
  }
  if (value.IsFloat()) {
    return std::to_string(value.GetFloatValue());
  }
  if (value.IsObject()) {
    const auto obj = value.GetObject();
    switch (obj->kind()) {
      case ObjectKind::kNativeFunction:
      case ObjectKind::kFunction:
        return absl::StrFormat("<function>@%d", IdentityHash(runtime, obj));
      case ObjectKind::kString: {
        const auto string_obj = static_cast<StringObject*>(obj);
        return std::string(string_obj->GetCharsPtr(), string_obj->length());
      }
      case ObjectKind::kArray: {
        const auto array = static_cast<ArrayObject*>(obj);
        std::string result = "[";
        for (int i = 0; i < array->length(); i++) {
          if (i > 0) {
            result += ", ";
          }
          result += ToString(runtime, array->GetElements()[i], depth + 1);
        }
        result += "]";
        return result;
      }
      case ObjectKind::kShapedObject: {
        const auto shaped = static_cast<ShapedObject*>(obj);
        const auto fields = shaped->GetFields();
        std::vector<std::pair<int, std::string_view>> field_list;
        auto* current = shaped->shape().Get();
        while (current != nullptr && current->slot_index() >= 0) {
          field_list.emplace_back(current->slot_index(),
                                  current->field_name()->ToStringView());
          current =
              current->parent().IsNull() ? nullptr : current->parent().Get();
        }
        std::sort(
            field_list.begin(), field_list.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        std::string result = "{";
        for (int i = 0; i < static_cast<int>(field_list.size()); i++) {
          if (i > 0) {
            result += ", ";
          }
          result += std::string(field_list[i].second) + ": " +
                    ToString(runtime, fields[field_list[i].first], depth + 1);
        }
        result += "}";
        return result;
      }
      default:
        return absl::StrFormat("<object>@%d", IdentityHash(runtime, obj));
    }
  }
  return "unknown";
}

int VMIntrinsics::IdentityHash(Runtime* runtime, const void* object) {
  const auto ptr = reinterpret_cast<uintptr_t>(object);
  return (ptr >> 32) + (ptr & 0xFFFF) * 13;
}
Value VMIntrinsics::Equals(VMThread* thread, Value left, Value right) {
  if (left.IsObject(ObjectKind::kString) &&
      right.IsObject(ObjectKind::kString)) {
    return Value::CreateBool(
        left.GetObjectUnchecked<StringObject>()->ToStringView() ==
        right.GetObjectUnchecked<StringObject>()->ToStringView());
  }
  return BinIntOp(thread, left, right, std::equal_to<uint64_t>{});
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
