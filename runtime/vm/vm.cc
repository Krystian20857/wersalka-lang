//
// Created by nothingbutyou on 5/17/26.
//

#include "vm.h"

namespace wersalka {
namespace lang {
namespace runtime {
Value VMThread::PopStack() {
  CHECK_GT(stack_top_, stack_.data());
  if (stack_top_ - 1 <= stack_.data()) {
    // TODO: Exception stack underflow
    SetPendingException(Value::CreateNull());
  }
  return *(--stack_top_);
}
Value VMThread::PeekStack() {
  CHECK_GT(stack_top_, stack_.data());
  return *(stack_top_ - 1);
}
void VMThread::PushStack(Value value) {
  if (stack_top_ >= stack_.data() + kMaxStackSize) {
    // TODO: Exception stack overflow
    SetPendingException(Value::CreateNull());
    return;
  }
  *(stack_top_++) = value;
}
void VMThread::SetStackTop(Value* value) { stack_top_ = value; }
Value* VMThread::GetStackTop() const { return stack_top_; }
VMFrame* VMThread::PushFrame(FunctionObject* function, CodeObject* code,
                             Value* locals) {
  if (frame_count_ >= kMaxFrameSize) {
    // TODO: Exception, stack overflow
    SetPendingException(Value::CreateNull());
    return nullptr;
  }
  const auto frame = &frames_[++frame_count_];
  frame->code_obj = code;
  frame->func_obj = function;
  frame->locals = locals;
  frame->pc = 0;
  return frame;
}
VMFrame VMThread::PopFrame() { return frames_[frame_count_--]; }
VMFrame* VMThread::CurrentFrame() { return &frames_[frame_count_]; }
void VMThread::SetPendingException(Value value) {
  Unwind(value);
}
Value VMThread::GetCurrentException() { return pending_exception_; }
void VMThread::SetThreadState(VMThreadState state) { thread_state_ = state; }
VMThreadState VMThread::GetThreadState() const { return thread_state_; }
void VMThread::Unwind(Value exception) {
  // TODO: exception handlers here
  while (frame_count_ > 0) {
    PopFrame();
  }
  pending_exception_ = exception;
  SetThreadState(VMThreadState::kError);
}

Value VMInterpreter::Execute(VMThread* thread, FunctionObject* entry) {
  thread->SetThreadState(VMThreadState::kRunning);

  // callee
  thread->PushStack(Value::CreateObject(entry));

  if (!CallFunction(thread, entry, 0)) {
    return thread->GetCurrentException();
  }

  return Run(thread);
}
Value VMInterpreter::Run(VMThread* thread) {
  while (thread->GetThreadState() == VMThreadState::kRunning) {
    const auto frame = thread->CurrentFrame();
    const auto code_object = frame->code_obj;
    const auto instr = code_object->instructions[frame->pc];

    // TODO: interpreter safety, bound checks, exceptions etc
    //  for now interpreter is quite barebone
    switch (instr.op) {
      case Opcode::kNop: {
        frame->pc++;
        break;
      }
      case Opcode::kPushConst: {
        const auto constant =
            MaterializeConstant(code_object->constants[instr.c2]);
        thread->PushStack(constant);
        frame->pc++;
        break;
      }
      case Opcode::kLoadLocal: {
        thread->PushStack(frame->locals[instr.c1]);
        frame->pc++;
        break;
      }
      case Opcode::kStoreLocal: {
        frame->locals[instr.c1] = thread->PopStack();
        frame->pc++;
        break;
      }
      case Opcode::kLoadGlobal: {
        const auto name = code_object->constants[instr.c2];
        CHECK(name.kind == ConstantDesc::Kind::kString);
        if (auto global = thread->globals_.find(name.str_v);
            global != thread->globals_.end()) {
          thread->PushStack(global->second);
        } else {
          thread->PushStack(runtime_->LookupGlobal(name.str_v));
        }
        frame->pc++;
        break;
      }
      case Opcode::kStoreGlobal: {
        const auto name = code_object->constants[instr.c2];
        CHECK(name.kind == ConstantDesc::Kind::kString);
        thread->globals_[name.str_v] = thread->PopStack();
        frame->pc++;
        break;
      }
      case Opcode::kJmp: {
        frame->pc = instr.c2;
        break;
      }
      case Opcode::kJmpIfTrue: {
        const auto value = thread->PopStack();
        if (VMIntrinsics::IsTruthful(value)) {
          frame->pc = instr.c2;
        } else {
          frame->pc++;
        }
        break;
      }
      case Opcode::kJmpIfFalse: {
        const auto value = thread->PopStack();
        if (!VMIntrinsics::IsTruthful(value)) {
          frame->pc = instr.c2;
        } else {
          frame->pc++;
        }
        break;
      }
      case Opcode::kPop: {
        thread->PopStack();
        frame->pc++;
        break;
      }
      case Opcode::kDup: {
        const auto value = thread->PopStack();
        thread->PushStack(value);
        thread->PushStack(value);
        frame->pc++;
        break;
      }
      case Opcode::kSwap: {
        const auto v1 = thread->PopStack();
        const auto v2 = thread->PopStack();
        thread->PushStack(v2);
        thread->PushStack(v1);
        frame->pc++;
        break;
      }
      case Opcode::kAdd: {
        ExecuteBinIntOp(thread, frame, std::plus<uint64_t>{});
        break;
      }
      case Opcode::kSub: {
        ExecuteBinIntOp(thread, frame, std::minus<uint64_t>{});
        break;
      }
      case Opcode::kMul: {
        ExecuteBinIntOp(thread, frame, std::multiplies<uint64_t>{});
        break;
      }
      case Opcode::kDiv: {
        ExecuteBinIntOp(thread, frame, std::divides<uint64_t>{});
        break;
      }
      case Opcode::kMod: {
        ExecuteBinIntOp(thread, frame, std::modulus<uint64_t>{});
        break;
      }
      case Opcode::kAnd: {
        ExecuteBinIntOp(thread, frame, std::bit_and<uint64_t>{});
        break;
      }
      case Opcode::kOr: {
        ExecuteBinIntOp(thread, frame, std::bit_or<uint64_t>{});
        break;
      }
      case Opcode::kXor: {
        ExecuteBinIntOp(thread, frame, std::bit_xor<uint64_t>{});
        break;
      }
      case Opcode::kShl: {
        ExecuteBinIntOp(thread, frame,
                        [](const auto a, const auto b) { return a << b; });
        break;
      }
      case Opcode::kShr: {
        ExecuteBinIntOp(thread, frame,
                        [](const auto a, const auto b) { return a >> b; });
        break;
      }
      case Opcode::kCmpGt: {
        ExecuteBinIntOp(thread, frame, std::greater<uint64_t>{});
        break;
      }
      case Opcode::kCmpLt: {
        ExecuteBinIntOp(thread, frame, std::less<uint64_t>{});
        break;
      }
      case Opcode::kCmpGe: {
        ExecuteBinIntOp(thread, frame, std::greater_equal<uint64_t>{});
        break;
      }
      case Opcode::kCmpLe: {
        ExecuteBinIntOp(thread, frame, std::less_equal<uint64_t>{});
        break;
      }
      case Opcode::kCmpEq: {
        ExecuteBinIntOp(thread, frame, std::equal_to<uint64_t>{});
        break;
      }
      case Opcode::kNeg: {
        const auto value = thread->PopStack();
        thread->PushStack(VMIntrinsics::Negate(thread, value));
        frame->pc++;
        break;
      }
      case Opcode::kInvoke: {
        const int arg_count = instr.c1;
        const auto callee = *(thread->GetStackTop() - arg_count - 1);

        if (!callee.IsObject()) {
          // TODO: Exception, invalid callee value
          thread->SetPendingException(Value::CreateNull());
          frame->pc++;
          break;
        }

        const auto obj = callee.GetObject();
        if (obj->kind() == ObjectKind::kFunction) {
          auto* fn = static_cast<FunctionObject*>(obj);
          CallFunction(thread, fn, arg_count);
        } else if (obj->kind() == ObjectKind::kNativeFunction) {
          auto* fn = static_cast<NativeFunctionObject*>(obj);
          if (arg_count >=
              static_cast<int>(thread->native_args_buffer_.size())) {
            // TODO: Exception, too many arguments
            thread->SetPendingException(Value::CreateNull());
            frame->pc++;
            break;
          }
          for (int n = arg_count - 1; n >= 0; n--) {
            thread->native_args_buffer_[n] = thread->PopStack();
          }
          thread->PopStack();  // pop callee

          auto native_context =
              NativeContext{.runtime = runtime_, .exception = nullptr};
          thread->SetThreadState(VMThreadState::kNative);
          const auto result = fn->handler(
              &native_context, {thread->native_args_buffer_.data(),
                                static_cast<std::size_t>(arg_count)});
          if (native_context.exception != nullptr) {
            thread->SetPendingException(*native_context.exception);
          } else {
            thread->SetThreadState(VMThreadState::kRunning);
            thread->PushStack(result);
          }
          frame->pc++;
        } else {
          // TODO: Exception, not callable
          thread->SetPendingException(Value::CreateNull());
          frame->pc++;
        }
        break;
      }
      case Opcode::kReturn: {
        const auto value = thread->PopStack();
        const auto prev_locals = thread->CurrentFrame()->locals;
        thread->PopFrame();
        if (thread->frame_count_ == 0) {
          return value;
        }
        thread->SetStackTop(prev_locals - 1);  // discard callee + args
        thread->PushStack(value);
        thread->CurrentFrame()->pc++;  // advance past kInvoke in caller
        break;
      }
      case Opcode::kReserved: {
        ABSL_UNREACHABLE();
      }
    }
  }
  // TODO: wrap exception in error type
  return thread->GetCurrentException();
}
bool VMInterpreter::CallFunction(VMThread* thread, FunctionObject* function,
                                 int arg_count) {
  const auto code = function->code_obj;

  if (code->arg_count != arg_count) {
    // TODO: exception
    thread->SetPendingException(Value::CreateNull());
    return false;
  }

  // see frame call conv
  // first args, then locals
  const auto locals = thread->GetStackTop() - arg_count;
  const auto frame = thread->PushFrame(function, code, locals);

  // non-arg locals, just like JVM
  for (int n = code->arg_count; n < code->max_locals; ++n) {
    locals[n] = Value::CreateNull();
  }

  thread->SetStackTop(frame->locals + code->max_locals);
  return true;
}
Value VMInterpreter::MaterializeConstant(ConstantDesc desc) {
  switch (desc.kind) {
    // TODO: based on Int value, allocate on heap as boxed value
    case ConstantDesc::Kind::kInt:
      return Value::CreateInt(desc.int_v);
    case ConstantDesc::Kind::kUInt:
      return Value::CreateInt(desc.uint_v);
    case ConstantDesc::Kind::kFloat: {
      // TODO: implement
      ABSL_UNREACHABLE();
    }
    case ConstantDesc::Kind::kBool:
      return Value::CreateBool(desc.bool_v);
    case ConstantDesc::Kind::kNull:
      return Value::CreateNull();
    case ConstantDesc::Kind::kString: {
      // TODO: implement
      ABSL_UNREACHABLE();
    }
    default: {
      ABSL_UNREACHABLE();
    }
  }
}
std::optional<int64_t> VMIntrinsics::CoerceToInt(Value value) {
  if (value.IsInt()) {
    return value.GetIntValue();
  }
  if (value.IsBool()) {
    return value.GetBoolValue() ? 1 : 0;
  }
  if (value.IsNull()) {
    return 0;
  }
  return std::nullopt;
}
std::optional<int64_t> VMIntrinsics::CoerceToBool(Value value) {
  if (value.IsBool()) {
    return value.GetBoolValue();
  }
  if (value.IsInt()) {
    return value.GetIntValue() > 0;
  }
  if (value.IsNull()) {
    return false;
  }
  return std::nullopt;
}
bool VMIntrinsics::IsTruthful(Value value) {
  if (value.IsNull()) {
    return false;
  }
  if (value.IsBool()) {
    return value.GetBoolValue();
  }
  if (value.IsInt()) {
    return value.GetIntValue() > 0;
  }
  return false;
}
Value VMIntrinsics::Negate(VMThread* thread, Value value) {
  const auto value_coerced = CoerceToInt(value);
  if (!value_coerced) {
    // TODO: Exception
    thread->SetPendingException(Value::CreateNull());
    return Value::CreateNull();
  }
  return Value::CreateInt(-(*value_coerced));
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
