//
// Created by nothingbutyou on 5/24/26.
//

#ifndef WERSALKALANG_MARK_SWEEP_H
#define WERSALKALANG_MARK_SWEEP_H

#include "runtime/gc/gc.h"
#include "runtime/zone.h"

// `MarkSweepGC` support multiple slabs and large objects for allocation
// each store medium have different header layout, common object header is
// defined in `ObjectHeader` struct.
//
// For `LargeObjectHeader` and `SlabObjectHeader` memory layout looks like that
// |-------------------|--------------|-------------|
//   LargeObjectHeader/  ObjectHeader ^  object data
//    SlabObjectHeader                |
//                                    |
//                                  GCPtr<Object> is here
//
// so to recover specific header pointer we just have to walk back
// based on metadata, especially `is_large_object` which defines the very first
// header type

namespace wersalka {
namespace lang {
namespace runtime {

class MarkSweepGC;
class MarkSweepGCVisitor;

class MarkSweepGC;

// not thread-safe
// without profiling slab shrinking is pointless
class AllocSlab {
 public:
  explicit AllocSlab(const int block_size)
      : block_size_(block_size), pages_(nullptr), free_block_(nullptr) {}

  // size = sizeof(HeapObject) + sizeof(Data),
  // always uses HeapObject alignment
  void* Alloc(std::size_t size);
  uint64_t Sweep();

  ~AllocSlab();

 private:
  friend class MarkSweepGC;

  static constexpr auto kDefaultPageSize = 1024;  // number of blocks, not bytes

  struct SlabObjectHeader {
    // empty
  };

  struct Page {
    Page* next;
    int32_t capacity;  // size in bytes
    int32_t bump;
  };

  struct FreeBlock {
    FreeBlock* next;
  };

  static std::size_t SizeOf(std::size_t object_size);
  static void* PtrOf(SlabObjectHeader* header);

  int block_size_;
  Page* pages_;
  FreeBlock* free_block_;
};

class MarkSweepGC : public GC {
 public:
  MarkSweepGC();
  ~MarkSweepGC() override;

  void* Alloc(std::size_t size, std::size_t align) override;
  void Collect(VMThread* thread) override;
  GCStats GetStats() const override;

 private:
  friend class AllocSlab;
  friend class MarkSweepGCVisitor;

  struct ObjectHeader {
    bool marked;
    bool is_large_object;
  };

  struct LargeObjectHeader {
    LargeObjectHeader* next;
    int32_t size;
    // byte offset from this header to the `ObjectHeader` immediately preceding
    // object data, stored to support variable alignment padding between
    // `LargeObjectHeader` and `ObjectHeader`.
    uint32_t obj_header_offset;
  };

  // TODO: configurable
  static constexpr auto kSlabSizes =
      std::array{8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
  static constexpr uint64_t kInitialGcThreshold = 10 * 1024 * 1024;  // 10mb

  void CollectNow(VMThread* thread);
  void OnAllocation(std::size_t size);
  AllocSlab* FindSlab(std::size_t size) const;

  static ObjectHeader* HeaderOf(GCPtr<Object> ptr);
  static void* PtrOfObjectFromHeader(ObjectHeader* header);
  static void* PtrOfObjectFromLarge(LargeObjectHeader* header);
  static ObjectHeader* PtrOfHeaderFromLarge(LargeObjectHeader* header);

  AllocSlab* alloc_slabs_;
  LargeObjectHeader* large_objects_;
  bool collect_requested_;
  uint64_t collect_threshold_;
  uint64_t allocated_bytes_;
  uint64_t alive_bytes_;
};

class MarkSweepGCVisitor : public GCVisitor {
 public:
  ~MarkSweepGCVisitor() override = default;
  bool Visit(Handle<Object> value) override;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_MARK_SWEEP_H
