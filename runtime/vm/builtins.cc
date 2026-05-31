//
// Created by nothingbutyou on 5/31/26.
//

#include "runtime/vm/builtins.h"

#include "runtime/vm/vm.h"

namespace wersalka {
namespace lang {
namespace runtime {
GCPtr<ShapedObject> FixedShape::New() const {
  return ShapedObject::New(runtime_->gc(), this->shape());
}
GCPtr<ShapedObject> FixedShape::New(
    const std::initializer_list<Field> fields) const {
  const auto object = New();
  for (const auto field : fields) {
    Set(object, field.name, field.value);
  }
  return object;
}
void FixedShape::Set(const GCPtr<ShapedObject> object,
                     const std::string_view field, const Value value) const {
  object->GetFields()[offset_cache_.at(field)] = value;
}
Value FixedShape::Get(const GCPtr<ShapedObject> object,
                      const std::string_view field) const {
  return object->GetFields()[offset_cache_.at(field)];
}
void Builtins::RegisterBuiltIns() const {
  runtime_->BindGlobalFunction("len", 1, [](auto _, auto args) {
    const auto arg0 = args[0];
    const auto array_object = arg0.template GetObject<ArrayObject>();
    if (array_object) {
      return Value::CreateInt((*array_object)->length());
    } else {
      return Value::CreateNull();
    }
  });
  runtime_->BindGlobalFunction(
      "cast", 2, [](NativeContext* ctx, auto args) -> Value {
        const auto arg0 = args[0];
        const auto arg1 = args[1];
        const auto is_string =
            arg1.IsObject() && arg1.GetObject()->kind() == ObjectKind::kString;
        if (!is_string) {
          *ctx->exception =
              ctx->runtime->NewException("Invalid cast type, string required");
          return Value::CreateNull();
        }
        const auto cast_type =
            arg1.template GetObjectUnchecked<StringObject>()->ToStringView();
        if (cast_type == "int") {
          return VMIntrinsics::CoerceToInt(arg0)
              .transform([](const auto it) { return Value::CreateInt(it); })
              .value_or(Value::CreateNull());
        } else if (cast_type == "float") {
          return VMIntrinsics::CoerceToFloat(arg0)
              .transform([](const auto it) { return Value::CreateFloat(it); })
              .value_or(Value::CreateNull());
        } else if (cast_type == "string") {
          return Value::CreateObject(
              VMIntrinsics::CoerceToString(ctx->runtime, arg0));
        } else if (cast_type == "bool") {
          return VMIntrinsics::CoerceToBool(arg0)
              .transform([](const auto it) { return Value::CreateBool(it); })
              .value_or(Value::CreateNull());
        } else {
          *ctx->exception = ctx->runtime->NewException(
              "Unknown cast type, allowed: `int`, `float`, `string`, `bool`");
          *ctx->exception = Value::CreateNull();
          return Value::CreateNull();
        }
      });
  runtime_->BindGlobalFunction(
      "__gc_stats", 0, [](NativeContext* ctx, auto args) -> Value {
        const auto gc = ctx->runtime->gc();
        const auto shape = ctx->runtime->builtins()->Shape_GCStats;
        // clang-format off
        const auto object = shape.New({
          {"allocated_bytes", Value::CreateInt(gc->GetStats().allocated_bytes)},
          {"alive_bytes", Value::CreateInt(gc->GetStats().alive_bytes)}
        });
        // clang-format on
        return Value::CreateObject(object);
      });
}
FixedShape Builtins::CreateFixedShape(
    std::initializer_list<std::string_view> fields) const {
  const auto fields_array = ArrayObject::NewStringArray(
      runtime_->gc(), {fields.begin(), fields.end()});
  const auto shape = runtime_->shaped_tree()->ShapeOf(fields_array);
  auto fixed_shape = FixedShape(runtime_, shape);
  for (const auto field : fields) {
    const auto interned_field =
        runtime_->GetPermanentZone()->InternString(field);
    fixed_shape.offset_cache_[interned_field] =
        runtime_->shaped_tree()->OffsetOf(shape, field);
  }
  return fixed_shape;
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
