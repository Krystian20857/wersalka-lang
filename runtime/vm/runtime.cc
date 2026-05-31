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
