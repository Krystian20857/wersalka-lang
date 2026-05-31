//
// Created by nothingbutyou on 5/17/26.
//

#include "gc.h"

#include <cstdlib>

#include "runtime/vm/object_impl.h"
#include "runtime/vm/vm.h"

namespace wersalka {
namespace lang {
namespace runtime {
void GCVisitor::WalkObject(Value* value) {
  if (!value->IsObject()) {
    return;
  }

  const auto handle = Handle<Object>(value);
  if (Visit(handle)) {
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
    case ObjectKind::kShapedObject: {
      const auto obj = static_cast<ShapedObject*>(handle.GetPtr());
      WalkObject(&obj->shape_.AsValue());
      WalkObject(&obj->values_.AsValue());
      break;
    }
    case ObjectKind::kValueArray: {
      const auto arr =
          static_cast<ShapedObject::ValueArray*>(handle.GetPtr());
      for (auto& slot : arr->GetSlots()) {
        WalkObject(&slot);
      }
      break;
    }
    case ObjectKind::kArray:
      break;
    case ObjectKind::kShape: {
      const auto shape = static_cast<Shape*>(handle.GetPtr());
      WalkObject(&shape->parent_.AsValue());
      WalkObject(&shape->field_name_.AsValue());
      WalkObject(&shape->transitions_.AsValue());
      break;
    }
    case ObjectKind::kTransitionArray: {
      const auto arr = static_cast<Shape::TransitionArray*>(handle.GetPtr());
      for (auto& t : arr->Get()) {
        WalkObject(&t.name.AsValue());
        WalkObject(&t.child.AsValue());
      }
      break;
    }
  }
}
void GCVisitor::WalkRoots(VMThread* thread) {
  // TODO: use interpreter stackmap
  //  eventually perform liveness analysis while compilation

  // frame walk
  for (auto n = 1; n <= thread->frame_count_; n++) {
    auto& frame = thread->frames_[n];
    WalkObject(&frame.func_obj.AsValue());

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

  for (auto* v = thread->handle_stack_.data(); v < thread->handle_top_; ++v) {
    WalkObject(v);
  }
}
void GCVisitor::WalkRoots(Runtime* runtime) {
  WalkObject(&runtime->shapes_.root_.AsValue());
  for (auto& [name, value] : runtime->builtin_globals_) {
    WalkObject(&value);
  }
  for (auto& [name, value] : runtime->functions_) {
    WalkObject(&value);
  }
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
