//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_GC_H
#define WERSALKALANG_GC_H

#include <cstddef>
#include <utility>

namespace wersalka {
namespace lang {
namespace runtime {

class VMThread;
class Runtime;
class GC;
class Value;
class Object;

template <typename T>
class Handle;  // ugly af

template <typename T>
using GCPtr = T*;

class GC {
 public:
  virtual ~GC() = default;

  virtual void* Alloc(std::size_t size, std::size_t align) = 0;
  virtual void Collect(VMThread* thread) = 0;

  template <typename T, typename... Args>
  GCPtr<T> NewSized(std::size_t size, Args&&... args);

  template <typename T, typename... Args>
  GCPtr<T> New(Args&&... args);
};

class GCVisitor {
 public:
  virtual ~GCVisitor() = default;

  virtual bool Visit(Handle<Object> value) = 0;

  void WalkObject(Value* value);
  void WalkRoots(VMThread* thread);
  void WalkRoots(Runtime* runtime);
};

template <typename T, typename... Args>
GCPtr<T> GC::NewSized(const std::size_t size, Args&&... args) {
  auto* ptr = Alloc(size, alignof(T));
  return new (ptr) T(std::forward<Args>(args)...);
}
template <typename T, typename... Args>
GCPtr<T> GC::New(Args&&... args) {
  return NewSized<T>(sizeof(T), args...);
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_GC_H
