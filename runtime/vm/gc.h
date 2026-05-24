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
class GC;

template <typename T>
using GCPtr = T*;

template <typename T>
class GCHandle {
 public:
  friend class GC;

  explicit GCHandle(const GCPtr<T> ptr) : ptr_(ptr) {}

  GCPtr<T> ptr() const { return ptr_; }

 private:
  GCPtr<T> ptr_;
};

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

class MarkSweepGC : public GC {
 public:
  void* Alloc(std::size_t size, std::size_t align) override;
  void Collect(VMThread* thread) override;

 private:
  struct HeapObject {
    HeapObject* next;
  };
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
