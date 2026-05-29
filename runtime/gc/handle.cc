//
// Created by nothingbutyou on 5/29/26.
//

#include "runtime/gc/handle.h"

#include "runtime/vm/vm.h"

namespace wersalka {
namespace lang {
namespace runtime {

HandleScope::HandleScope(VMThread* thread)
    : thread_(thread), saved_top_(thread->handle_top_) {}

HandleScope::~HandleScope() { thread_->handle_top_ = saved_top_; }

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
