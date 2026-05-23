//
// Created by nothingbutyou on 5/17/26.
//

#include "runtime.h"
namespace wersalka::lang::runtime {
void Runtime::AddGlobalValue(const std::string_view name, const Value value) {
  builtin_globals_[zone_->InternString(name)] = value;
}
void Runtime::BindGlobalFunction(std::string_view name, int arg_count,
                                 NativeFunctionObject::HandlerFn handler) {
  builtin_globals_[zone_->InternString(name)] =
      Value::CreateObject(zone_->New<NativeFunctionObject>(arg_count, handler));
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
void Runtime::RegisterFunction(FunctionObject* function) {
  functions_[zone_->InternString(function->name)] = function;
}
std::optional<FunctionObject*> Runtime::LookupFunction(std::string_view name) {
  if (const auto fn = functions_.find(name); fn != functions_.end()) {
    return fn->second;
  } else {
    return std::nullopt;
  }
}
}  // namespace wersalka::lang::runtime
