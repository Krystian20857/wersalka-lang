//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_RUNTIME_H
#define WERSALKALANG_RUNTIME_H

#include "absl/container/flat_hash_map.h"
#include "runtime/gc/gc.h"
#include "runtime/gc/mark_sweep.h"
#include "runtime/vm/builtins.h"
#include "runtime/vm/code_object.h"
#include "runtime/vm/object_impl.h"
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

class NativeFunctionObject : public Object {
 public:
  static constexpr auto kKind = ObjectKind::kNativeFunction;

  using HandlerFn = Value (*)(NativeContext* context,
                              std::span<const Value> args);

  int arg_count() const { return arg_count_; }
  HandlerFn handler() const { return handler_; }

 private:
  explicit NativeFunctionObject(const int arg_count, const HandlerFn handler)
      : Object(ObjectKind::kNativeFunction),
        arg_count_(arg_count),
        handler_(handler) {}

  friend class GC;

  int arg_count_;  // -1, for entire stack
  HandlerFn handler_;
};

class Runtime {
 public:
  friend class GCVisitor;

  explicit Runtime(Zone* zone)
      : zone_(zone),
        gc_(std::make_unique<MarkSweepGC>()),
        shapes_(gc_.get()),
        builtins_(std::make_unique<Builtins>(this)) {}

  Zone* GetPermanentZone() const { return zone_; }

  void AddGlobalValue(std::string_view name, Value value);
  void BindGlobalFunction(std::string_view name, int arg_count,
                          NativeFunctionObject::HandlerFn handler);

  Value LookupGlobal(std::string_view name);

  template <typename... Args>
  ZonePtr<CodeObject> CreateCodeObject(Args... args);

  // TODO: ZonePtr<...> them
  void RegisterFunction(GCPtr<FunctionObject> function);
  std::optional<GCPtr<FunctionObject>> LookupFunction(std::string_view name);

  GC* gc() const { return gc_.get(); }
  const ShapeTree* shaped_tree() const { return &shapes_; }
  const Builtins* builtins() const { return builtins_.get(); }  // refs&&&&?

 private:
  Zone* zone_;
  absl::flat_hash_map<ZoneStr, Value> builtin_globals_;
  std::vector<ZonePtr<CodeObject>> code_objects;
  absl::flat_hash_map<ZoneStr, Value> functions_;
  std::unique_ptr<GC> gc_;
  ShapeTree shapes_;
  std::unique_ptr<Builtins> builtins_;
};

template <typename... Args>
ZonePtr<CodeObject> Runtime::CreateCodeObject(Args... args) {
  const auto code_object = zone_->New<CodeObject>(std::forward<Args>(args)...);
  code_objects.push_back(code_object);
  return code_object;
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_RUNTIME_H
