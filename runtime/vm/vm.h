//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_VM_H
#define WERSALKALANG_VM_H

#include "runtime/vm/code_object.h"
#include "runtime/vm/runtime.h"
#include "runtime/vm/value.h"

namespace wersalka {
namespace lang {
namespace runtime {

enum class VMThreadState { kRunning, kError, kReturned };

// call conv:
//  [callee] [arg0]...[argX] [local0]...[localX] [operand0]...[operandX]
struct VMFrame {
  VMFrame(CodeObject* code_obj, FunctionObject* func_obj, Value* locals)
      : code_obj(code_obj), func_obj(func_obj), pc(0), locals(locals) {}

  // keep VMThread::frames_ happy
  VMFrame() : VMFrame(nullptr, nullptr, nullptr) {}

  CodeObject* code_obj;
  FunctionObject* func_obj;
  uint32_t pc;
  Value* locals;
};

// TODO: handle tracking for moving GC
class VMThread {
 public:
  static constexpr auto kMaxStackSize = 1024 * 1024;
  static constexpr auto kMaxFrameSize = 4096;

  VMThread() : stack_top_(stack_.end()), frame_count_(0) {}

  Value PopStack();
  Value PeekStack();
  void PushStack(Value value);
  void SetStackTop(Value* value);
  Value* GetStackTop() const;

  VMFrame* PushFrame(FunctionObject* function, CodeObject* code, Value* locals);
  VMFrame PopFrame();
  VMFrame* CurrentFrame();

  void SetPendingException(Value value);
  Value GetCurrentException();
  void SetThreadState(VMThreadState state);
  VMThreadState GetThreadState() const;

 private:
  std::array<Value, kMaxStackSize> stack_;
  Value* stack_top_;

  std::array<VMFrame, kMaxFrameSize> frames_;
  int frame_count_;
  VMThreadState thread_state_;
  Value pending_exception_;
};

class VMInterpreter {
 public:
  explicit VMInterpreter(Runtime* runtime) : runtime_(runtime) {}

  Value Execute(VMThread* thread, FunctionObject* entry);

 private:
  Value Run(VMThread* thread);
  bool CallFunction(VMThread* thread, FunctionObject* function, int arg_count);
  Runtime* runtime_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_VM_H
