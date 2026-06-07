//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_VM_H
#define WERSALKALANG_VM_H

#include "runtime/gc/handle.h"
#include "runtime/vm/code_object.h"
#include "runtime/vm/object_impl.h"
#include "runtime/vm/runtime.h"
#include "runtime/vm/value.h"
#include "runtime/vm/vm_intrinsics.h"

namespace wersalka {
namespace lang {
namespace runtime {

class VMInterpreter;
class VMThread;

enum class VMThreadState { kRunning, kNative, kError, kReturned };

// call conv:
//  [callee] [arg0]...[argX] [local0]...[localX] [operand0]...[operandX]
struct VMFrame {
  VMFrame(CodeObject* code_obj, Tagged<FunctionObject> func_obj, Value* locals)
      : code_obj(code_obj),
        func_obj(func_obj),
        pc(0),
        locals(locals),
        current_context(Value::CreateNull()) {}

  // keep VMThread::frames_ happy
  VMFrame() : VMFrame(nullptr, Value::CreateNull(), nullptr) {}

  CodeObject* code_obj;
  Tagged<FunctionObject> func_obj;
  uint32_t pc;
  Value* locals;
  Tagged<ClosureContextObject> current_context;
};

class VMThread {
 public:
  friend class VMInterpreter;
  friend class GCVisitor;
  friend class HandleScope;

  static constexpr auto kMaxStackSize = 1024 * 1024;
  static constexpr auto kMaxFrameSize = 4096;
  static constexpr auto kNativeArgumentBufferSize = 64;
  static constexpr auto kMaxHandles = 256;

  explicit VMThread(Runtime* runtime)
      : runtime_(runtime),
        stack_top_(stack_.data()),
        frame_count_(0),
        thread_state_(VMThreadState::kRunning),
        handle_top_(handle_stack_.data()) {}

  Runtime* runtime() const { return runtime_; }

  Value PopStack();
  Value PeekStack();
  void PushStack(Value value);
  void SetStackTop(Value* value);
  Value* GetStackTop() const;

  VMFrame* PushFrame(GCPtr<FunctionObject> function, CodeObject* code,
                     Value* locals);
  VMFrame PopFrame();
  VMFrame* CurrentFrame();

  void SetPendingException(Value value);
  Value GetCurrentException();
  void SetThreadState(VMThreadState state);
  VMThreadState GetThreadState() const;

  void Unwind(Value exception);
  void CaptureStackTrace();
  void ThrowException(Value value);

 private:
  Runtime* runtime_;
  std::array<Value, kMaxStackSize> stack_;
  Value* stack_top_;
  std::array<VMFrame, kMaxFrameSize> frames_;
  int frame_count_;
  VMThreadState thread_state_;
  Value pending_exception_;
  std::array<Value, kNativeArgumentBufferSize> native_args_buffer_;
  std::array<Value, kMaxHandles> handle_stack_;
  Value* handle_top_;
};

template <typename T>
Local<T> HandleScope::Alloc(GCPtr<T> ptr) {
  CHECK_LT(thread_->handle_top_,
           thread_->handle_stack_.data() + VMThread::kMaxHandles);
  *thread_->handle_top_ = Value::CreateObject(ptr);
  return Local<T>(thread_->handle_top_++);
}

class VMInterpreter {
 public:
  explicit VMInterpreter(Runtime* runtime) : runtime_(runtime) {}

  Value Execute(VMThread* thread, GCPtr<FunctionObject> entry);
  Value Execute(VMThread* thread, GCPtr<FunctionObject> entry,
                std::span<const Value> args);

 private:
  Value Run(VMThread* thread);
  bool CallFunction(VMThread* thread, GCPtr<FunctionObject> function,
                    int arg_count);

  Value MaterializeConstant(ConstantDesc desc) const;
  void ThrowRuntimeError(VMThread* thread, std::string_view message);

  template <typename Op>
  static void ExecuteBinIntOp(VMThread* thread, VMFrame* frame, Op op);

  template <typename Op>
  void ExecuteWildcardBinOp(VMThread* thread, VMFrame* frame, Op op);

  Runtime* runtime_;
};

template <typename Op>
void VMInterpreter::ExecuteBinIntOp(VMThread* thread, VMFrame* frame, Op op) {
  const auto right = thread->PopStack();
  const auto left = thread->PopStack();
  thread->PushStack(VMIntrinsics::BinIntOp(thread, left, right, op));
  frame->pc++;
}

template <typename Op>
void VMInterpreter::ExecuteWildcardBinOp(VMThread* thread, VMFrame* frame,
                                         Op op) {
  const auto right = thread->PopStack();
  const auto left = thread->PopStack();
  if (right.IsFloat() || left.IsFloat()) {
    thread->PushStack(VMIntrinsics::BinFloatOp(thread, left, right, op));
  } else {
    thread->PushStack(VMIntrinsics::BinIntOp(thread, left, right, op));
  }
  frame->pc++;
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_VM_H
