//
// Created by nothingbutyou on 5/17/26.
//

#include "vm.h"

namespace wersalka {
namespace lang {
namespace runtime {
Value VMThread::PopStack() {
  CHECK_GT(stack_top_, stack_.data());
  return *(--stack_top_);
}
Value VMThread::PeekStack() {
  CHECK(stack_top_ != nullptr);
  return *stack_top_;
}
void VMThread::PushStack(Value value) { *(++stack_top_) = value; }
void VMThread::SetStackTop(Value* value) { stack_top_ = value; }
Value* VMThread::GetStackTop() const { return stack_top_; }
VMFrame* VMThread::PushFrame(FunctionObject* function, CodeObject* code,
                             Value* locals) {
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
  pending_exception_ = value;
  SetThreadState(VMThreadState::kError);
}
Value VMThread::GetCurrentException() { return pending_exception_; }
void VMThread::SetThreadState(VMThreadState state) { thread_state_ = state; }
VMThreadState VMThread::GetThreadState() const { return thread_state_; }

Value VMInterpreter::Execute(VMThread* thread, FunctionObject* entry) {
  thread->SetThreadState(VMThreadState::kRunning);

  // callee
  thread->PushStack(Value::CreateObject(entry));

  if (!CallFunction(thread, entry, 0)) {
    return Value::CreateNull();
  }

  if (thread->GetThreadState() == VMThreadState::kReturned) {
    return thread->PopStack();
  }

  Run(thread);

  // TODO: wrap exception with some EvalError object
  //  otherwise it's unrecognizable from normal return
  return thread->GetCurrentException();
}
Value VMInterpreter::Run(VMThread* thread) {}
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
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
