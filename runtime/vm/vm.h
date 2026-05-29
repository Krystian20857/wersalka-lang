//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_VM_H
#define WERSALKALANG_VM_H

#include "runtime/gc/handle.h"
#include "runtime/vm/code_object.h"
#include "runtime/vm/runtime.h"
#include "runtime/vm/value.h"

namespace wersalka {
namespace lang {
namespace runtime {

class VMInterpreter;
class VMThread;

enum class VMThreadState { kRunning, kNative, kError, kReturned };

// call conv:
//  [callee] [arg0]...[argX] [local0]...[localX] [operand0]...[operandX]
struct VMFrame {
  VMFrame(CodeObject* code_obj, Value func_obj, Value* locals)
      : code_obj(code_obj), func_obj(func_obj), pc(0), locals(locals) {}

  // keep VMThread::frames_ happy
  VMFrame() : VMFrame(nullptr, Value::CreateNull(), nullptr) {}

  CodeObject* code_obj;
  Value func_obj;
  uint32_t pc;
  Value* locals;
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

 private:
  Runtime* runtime_;
  std::array<Value, kMaxStackSize> stack_;
  Value* stack_top_;
  std::array<VMFrame, kMaxFrameSize> frames_;
  int frame_count_;
  VMThreadState thread_state_;
  Value pending_exception_;
  absl::flat_hash_map<ZoneStr, Value> globals_;
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

 private:
  Value Run(VMThread* thread);
  bool CallFunction(VMThread* thread, GCPtr<FunctionObject> function,
                    int arg_count);

  Value MaterializeConstant(ConstantDesc desc) const;

  template <typename Op>
  static void ExecuteBinIntOp(VMThread* thread, VMFrame* frame, Op op);

  Runtime* runtime_;
};

class VMIntrinsics {
 public:
  static std::optional<int64_t> CoerceToInt(Value value);
  static std::optional<int64_t> CoerceToBool(Value value);
  static GCPtr<StringObject> CoerceToString(VMThread* thread, Value value);

  static bool IsTruthful(Value value);
  static Value Add(VMThread* thread, Value left, Value right);
  static Value Negate(VMThread* thread, Value value);
  static std::string ToString(Runtime* runtime, Value value);
  static int IdentityHash(Runtime* runtime, Object* object);

  // lovely template
  template <typename Op>
  static Value BinIntOp(VMThread* thread, Value left, Value right, Op op);
};

template <typename Op>
void VMInterpreter::ExecuteBinIntOp(VMThread* thread, VMFrame* frame, Op op) {
  const auto right = thread->PopStack();
  const auto left = thread->PopStack();
  thread->PushStack(VMIntrinsics::BinIntOp(thread, left, right, op));
  frame->pc++;
}

template <typename Op>
Value VMIntrinsics::BinIntOp(VMThread* thread, Value left, Value right, Op op) {
  const auto left_coerced = CoerceToInt(left);
  const auto right_coerced = CoerceToInt(right);
  if (!left_coerced || !right_coerced) {
    thread->SetPendingException(Value::CreateNull());
    return Value::CreateNull();
  }
  using Result = std::invoke_result_t<Op, int64_t, int64_t>();
  if constexpr (std::is_same_v<Result, bool>) {
    return Value::CreateBool(op(*left_coerced, *right_coerced));
  } else {
    return Value::CreateInt(op(*left_coerced, *right_coerced));
  }
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_VM_H
