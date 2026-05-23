//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_RUNTIME_H
#define WERSALKALANG_RUNTIME_H

#include "absl/container/flat_hash_map.h"
#include "runtime/vm/code_object.h"
#include "runtime/vm/value.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

class Runtime;

struct NativeContext {
  Runtime* runtime;
  Value* exception;
};

struct NativeFunctionObject : public Object {
  static constexpr auto kKind = ObjectKind::kNativeFunction;

  using HandlerFn = Value (*)(NativeContext* context,
                              std::span<const Value> args);

  NativeFunctionObject(const int arg_count, const HandlerFn handler)
      : Object(ObjectKind::kNativeFunction),
        arg_count(arg_count),
        handler(handler) {}

  int arg_count;  // -1, for entire stack
  HandlerFn handler;
};

class Runtime {
 public:
  explicit Runtime(Zone* zone) : zone_(zone) {}

  Zone* GetPermanentZone() const { return zone_; }

  void AddGlobalValue(std::string_view name, Value value);
  void BindGlobalFunction(std::string_view name, int arg_count,
                          NativeFunctionObject::HandlerFn handler);

  Value LookupGlobal(std::string_view name);

  ZonePtr<CodeObject> CreateCodeObject(
      const std::span<const Instr>& instructions,
      const std::span<const ConstantDesc>& constants, uint32_t arg_count,
      uint32_t max_stack, uint32_t max_locals);

  // TODO: ZonePtr<...> them
  void RegisterFunction(FunctionObject* function);
  std::optional<FunctionObject*> LookupFunction(std::string_view name);

 private:
  Zone* zone_;
  absl::flat_hash_map<ZoneStr, Value> builtin_globals_;
  std::vector<ZonePtr<CodeObject>> code_objects;
  absl::flat_hash_map<ZoneStr, FunctionObject*> functions_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_RUNTIME_H
