//
// Created by nothingbutyou on 5/17/26.
//

#include "gc.h"

#include <cstdlib>

#include "runtime/vm/vm.h"

namespace wersalka {
namespace lang {
namespace runtime {
void GCVisitor::WalkObject(Value* value) {
  if (!value->IsObject()) {
    return;
  }

  const auto handle = Handle<Object>(value);
  if (!Visit(handle)) {
    return;
  }

  // TODO: walk children here
  switch (handle.GetPtr()->kind()) {
    case ObjectKind::kFunction:
      break;
    case ObjectKind::kNativeFunction:
      break;
    case ObjectKind::kBigInt:
      break;
    case ObjectKind::kString:
      break;
    case ObjectKind::kShapedObject:
      break;
    case ObjectKind::kObjectArray:
      break;
  }
}
void GCVisitor::WalkRoots(VMThread* thread) {
  // frame walk
  for (auto n = 1; n <= thread->frame_count_; n++) {
    const auto& frame = thread->frames_[n];

    // TODO: should frame keep function `Value` reference?,
    //   so I can implement moving collector later
    auto value_stub = Value::CreateObject<FunctionObject>(frame.func_obj);
    Visit(Handle<Object>(&value_stub));

    // TODO: is it correct?
    //  I guess for fully correct and performant stack walk I would have to
    //  compute some kind of stackmap for every bci, for now it should be ok
    auto* start = frame.locals;
    auto* end = (n == thread->frame_count_) ? thread->stack_top_
                                            : thread->frames_[n + 1].locals - 1;
    for (auto v = start; v < end; ++v) {
      WalkObject(v);
    }
  }

  for (auto& [name, global] : thread->globals_) {
    WalkObject(&global);
  }
}
void GCVisitor::WalkRoots(Runtime* runtime) {
  for (auto& [name, value] : runtime->builtin_globals_) {
    WalkObject(&value);
  }
  for (auto [name, value] : runtime->functions_) {
    // idk is that correct
    auto value_stub = Value::CreateObject<FunctionObject>(value);
    Visit(Handle<Object>(&value_stub));
  }
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
