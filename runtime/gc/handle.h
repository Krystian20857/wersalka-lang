//
// Created by nothingbutyou on 5/29/26.
//

#ifndef WERSALKALANG_HANDLE_H
#define WERSALKALANG_HANDLE_H

#include "runtime/gc/gc.h"
#include "runtime/vm/value.h"

namespace wersalka {
namespace lang {
namespace runtime {

class VMThread;

// Local<...> use slots from VMThread::handle_stack_
template <typename T>
class Local {
 public:
  GCPtr<T> Get() const {
    if (!value_->IsObject()) {
      return nullptr;
    }
    return static_cast<T*>(value_->GetObject());
  }
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }
  // implicit conversion so Local<T> can be passed to raw-pointer APIs
  operator GCPtr<T>() const { return Get(); }

 private:
  friend class HandleScope;

  explicit Local(Value* slot) : value_(slot) {}
  Value* value_;
};

class HandleScope {
 public:
  explicit HandleScope(VMThread* thread);
  ~HandleScope();

  HandleScope(const HandleScope&) = delete;
  HandleScope& operator=(const HandleScope&) = delete;
  HandleScope(HandleScope&&) = delete;
  HandleScope& operator=(HandleScope&&) = delete;

  template <typename T>
  Local<T> Alloc(GCPtr<T> ptr);

 private:
  VMThread* thread_;
  Value* saved_top_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_HANDLE_H
