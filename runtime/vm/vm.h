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

class VMInterpreter;
class VMThread;

enum class VMThreadState { kRunning, kNative, kError, kReturned };

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
  friend class VMInterpreter;

  static constexpr auto kMaxStackSize = 1024 * 1024;
  static constexpr auto kMaxFrameSize = 4096;
  static constexpr auto kNativeArgumentBufferSize = 64;

  VMThread()
      : stack_top_(stack_.data()),
        frame_count_(0),
        thread_state_(VMThreadState::kRunning) {}

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

  void Unwind(Value exception);

 private:
  std::array<Value, kMaxStackSize> stack_;
  Value* stack_top_;
  std::array<VMFrame, kMaxFrameSize> frames_;
  int frame_count_;
  VMThreadState thread_state_;
  Value pending_exception_;
  absl::flat_hash_map<ZoneStr, Value> globals_;
  std::array<Value, kNativeArgumentBufferSize> native_args_buffer_;
};

class VMInterpreter {
 public:
  explicit VMInterpreter(Runtime* runtime) : runtime_(runtime) {}

  Value Execute(VMThread* thread, FunctionObject* entry);

 private:
  Value Run(VMThread* thread);
  bool CallFunction(VMThread* thread, FunctionObject* function, int arg_count);

  Value MaterializeConstant(ConstantDesc desc);

  template <typename Op>
  static void ExecuteBinIntOp(VMThread* thread, VMFrame* frame, Op op);

  Runtime* runtime_;
};

class VMIntrinsics {
 public:
  static std::optional<int64_t> CoerceToInt(Value value);
  static std::optional<int64_t> CoerceToBool(Value value);

  static bool IsTruthful(Value value);
  static Value Negate(VMThread* thread, Value value);

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
