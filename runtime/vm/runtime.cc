//
// Created by nothingbutyou on 5/17/26.
//

#include "runtime/vm/runtime.h"

#include "runtime/vm/vm.h"

namespace wersalka::lang::runtime {
void Runtime::AddGlobalValue(const std::string_view name, const Value value) {
  builtin_globals_[zone_->InternString(name)] = value;
}
void Runtime::BindGlobalFunction(std::string_view name, int arg_count,
                                 NativeFunctionObject::HandlerFn handler) {
  builtin_globals_[zone_->InternString(name)] =
      Value::CreateObject(gc_->New<NativeFunctionObject>(arg_count, handler));
}
Value Runtime::LookupGlobal(std::string_view name) {
  if (auto global = builtin_globals_.find(name);
      global != builtin_globals_.end()) {
    return global->second;
  }
  if (auto func = LookupFunction(name)) {
    return Value::CreateObject(*func);
  }
  return Value::CreateNull();
}
ZonePtr<CodeObject> Runtime::CreateCodeObject(
    const std::span<const Instr>& instructions,
    const std::span<const ConstantDesc>& constants, uint32_t arg_count,
    uint32_t max_stack, uint32_t max_locals) {
  const auto code_object = zone_->New<CodeObject>(
      instructions, constants, arg_count, max_stack, max_locals);
  code_objects.push_back(code_object);
  return code_object;
}
void Runtime::RegisterBuiltIns() {
  BindGlobalFunction("len", 1, [](auto _, auto args) {
    const auto arg0 = args[0];
    const auto array_object = arg0.template GetObject<ArrayObject>();
    if (array_object) {
      return Value::CreateInt((*array_object)->length());
    } else {
      return Value::CreateNull();
    }
  });
  BindGlobalFunction("cast", 2, [](auto ctx, auto args) -> Value {
    const auto arg0 = args[0];
    const auto arg1 = args[1];
    const auto is_string =
        arg1.IsObject() && arg1.GetObject()->kind() == ObjectKind::kString;
    if (!is_string) {
      // TODO: Exception, invalid value type
      *ctx->exception = Value::CreateNull();
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
      // TODO: Exception, invalid cast type
      *ctx->exception = Value::CreateNull();
      return Value::CreateNull();
    }
  });
  BindGlobalFunction(
      "__gc_stats", 0, [](NativeContext* ctx, auto args) -> Value {
        const auto gc = ctx->runtime->gc();
        static constexpr std::string_view kGcStatsFieldNames[] = {
            "allocated_bytes", "alive_bytes"};
        const auto fields = ArrayObject::NewStringArray(gc, kGcStatsFieldNames);
        const auto shape_tree = ctx->runtime->shaped_tree();
        const auto shape = shape_tree->ShapeOf(fields);
        const auto object = ShapedObject::New(gc, shape);
        object->GetFields()[shape_tree->OffsetOf(shape, "allocated_bytes")] =
            Value::CreateInt(gc->GetStats().allocated_bytes);
        object->GetFields()[shape_tree->OffsetOf(shape, "alive_bytes")] =
            Value::CreateInt(gc->GetStats().alive_bytes);
        return Value::CreateObject(object);
      });
}
void Runtime::RegisterFunction(FunctionObject* function) {
  functions_[zone_->InternString(function->name())] =
      Value::CreateObject(function);
}
std::optional<FunctionObject*> Runtime::LookupFunction(std::string_view name) {
  if (const auto fn = functions_.find(name); fn != functions_.end()) {
    return static_cast<FunctionObject*>(fn->second.GetObject());
  } else {
    return std::nullopt;
  }
}
}  // namespace wersalka::lang::runtime
