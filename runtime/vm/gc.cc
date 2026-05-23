//
// Created by nothingbutyou on 5/17/26.
//

#include "gc.h"

#include <cstdlib>

namespace wersalka {
namespace lang {
namespace runtime {
void* MarkSweepGC::Alloc(std::size_t size, std::size_t align) {
  // TODO: IMPLEMENT GC
  return std::aligned_alloc(align, size);
}
void MarkSweepGC::Collect(VMThread* thread) {
  // TODO: IMPLEMENT GC
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
