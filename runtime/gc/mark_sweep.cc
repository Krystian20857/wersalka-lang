//
// Created by nothingbutyou on 5/24/26.
//

#include "mark_sweep.h"

#include "runtime/vm/vm.h"

namespace wersalka {
namespace lang {
namespace runtime {
namespace {
inline constexpr auto kLog2MinSize = 3;
inline constexpr auto kSlabAlign = 8;
}  // namespace

// TODO: right now I'm too lazy to implement os specific mempage
//  for now using std::aligned_alloc

void* AllocSlab::Alloc(std::size_t size) {
  CHECK_LE(size, block_size_);

  if (free_block_ != nullptr) {
    const auto free_block = free_block_;
    free_block_ = free_block->next;
    // free_block points into the object data area; walk back to ObjectHeader
    const auto object_header = reinterpret_cast<MarkSweepGC::ObjectHeader*>(
        reinterpret_cast<char*>(free_block) - sizeof(MarkSweepGC::ObjectHeader));
    object_header->marked = false;
    object_header->is_large_object = false;
    return free_block;
  }

  if (pages_ == nullptr || pages_->bump + block_size_ > pages_->capacity) {
    const auto capacity = kDefaultPageSize * block_size_;
    const auto new_page = static_cast<Page*>(
        std::aligned_alloc(alignof(Page), sizeof(Page) + capacity));
    new_page->next = pages_;
    new_page->capacity = capacity;
    // keep pointer aligned to block size
    new_page->bump = block_size_ - (sizeof(SlabObjectHeader) +
                                    sizeof(MarkSweepGC::ObjectHeader));
    pages_ = new_page;
  }

  const auto slab_header = reinterpret_cast<SlabObjectHeader*>(
      reinterpret_cast<char*>(pages_) + sizeof(Page) + pages_->bump);
  pages_->bump += block_size_;
  return PtrOf(slab_header);
}
uint64_t AllocSlab::Sweep() {
  // Rebuild the free list from scratch each sweep so blocks freed in a prior
  // sweep are naturally re-added without corrupting their next pointers.
  free_block_ = nullptr;
  uint64_t alive_bytes = 0;
  auto current_page = pages_;
  const auto first_offset =
      block_size_ - (sizeof(SlabObjectHeader) + sizeof(MarkSweepGC::ObjectHeader));
  while (current_page != nullptr) {
    for (auto offset = first_offset; offset < current_page->bump; offset += block_size_) {
      const auto slab_header_ptr = reinterpret_cast<SlabObjectHeader*>(
          reinterpret_cast<char*>(current_page) + sizeof(Page) + offset);
      const auto obj_header_ptr = reinterpret_cast<MarkSweepGC::ObjectHeader*>(
          reinterpret_cast<char*>(slab_header_ptr) + sizeof(SlabObjectHeader));
      if (!obj_header_ptr->marked) {
        // FreeBlock lives at the object data start (right after ObjectHeader),
        // so FreeBlock::next never overlaps ObjectHeader::marked.
        const auto free_block = reinterpret_cast<FreeBlock*>(
            reinterpret_cast<char*>(obj_header_ptr) + sizeof(MarkSweepGC::ObjectHeader));
        free_block->next = free_block_;
        free_block_ = free_block;
      } else {
        obj_header_ptr->marked = false;
        alive_bytes += block_size_;
      }
    }
    current_page = current_page->next;
  }
  return alive_bytes;
}
AllocSlab::~AllocSlab() {
  auto page = pages_;
  while (page != nullptr) {
    auto next = page->next;
    std::free(page);
    page = next;
  }
}
std::size_t AllocSlab::SizeOf(const std::size_t object_size) {
  return sizeof(SlabObjectHeader) + sizeof(MarkSweepGC::ObjectHeader) +
         object_size;
}
void* AllocSlab::PtrOf(SlabObjectHeader* header) {
  const auto obj_header = reinterpret_cast<MarkSweepGC::ObjectHeader*>(
      reinterpret_cast<char*>(header) + sizeof(SlabObjectHeader));
  obj_header->marked = false;
  obj_header->is_large_object = false;
  return reinterpret_cast<char*>(obj_header) + sizeof(MarkSweepGC::ObjectHeader);
}
void* MarkSweepGC::Alloc(const std::size_t size, const std::size_t align) {
  if (size > kSlabSizes[kSlabSizes.size() - 1]) {
    // place ObjectHeader immediately before data so `HeaderOf` can walk back
    // `sizeof(ObjectHeader)`, compute the offset from `LargeObjectHeader` to
    // `ObjectHeader` such that data `(ObjectHeader + 1)` is aligned to align.
    constexpr auto raw_data_start =
        sizeof(LargeObjectHeader) + sizeof(ObjectHeader);
    const auto aligned_data_start = (raw_data_start + align - 1) & ~(align - 1);
    const auto obj_header_offset =
        static_cast<uint32_t>(aligned_data_start - sizeof(ObjectHeader));
    const auto alloc_align = std::max(alignof(LargeObjectHeader), align);
    const auto alloc_size =
        (aligned_data_start + size + alloc_align - 1) & ~(alloc_align - 1);
    const auto large_object = static_cast<LargeObjectHeader*>(
        std::aligned_alloc(alloc_align, alloc_size));
    large_object->next = large_objects_;
    large_object->size = size;
    large_object->obj_header_offset = obj_header_offset;
    large_objects_ = large_object;
    const auto object_header = PtrOfHeaderFromLarge(large_object);
    object_header->marked = false;
    object_header->is_large_object = true;
    OnAllocation(size);
    return PtrOfObjectFromLarge(large_object);
  } else {
    const auto real_size = AllocSlab::SizeOf(size);
    OnAllocation(real_size);
    return FindSlab(real_size)->Alloc(real_size);
  }
}
void MarkSweepGC::Collect(VMThread* thread) {
  if (collect_requested_) {
    CollectNow(thread);
    allocated_bytes_ = alive_bytes_;
    collect_requested_ = false;
  }
}
GCStats MarkSweepGC::GetStats() const {
  return GCStats {
    .allocated_bytes = this->allocated_bytes_,
    .alive_bytes = this->alive_bytes_
  };
}

void MarkSweepGC::CollectNow(VMThread* thread) {
  alive_bytes_ = 0;

  // mark
  MarkSweepGCVisitor visitor;
  visitor.WalkRoots(thread->runtime());
  visitor.WalkRoots(thread);

  // sweep slab
  for (auto n = 0; n < kSlabSizes.size(); ++n) {
    alive_bytes_ += alloc_slabs_[n].Sweep();
  }

  // sweep large objects
  LargeObjectHeader* prev = nullptr;
  auto current_object = large_objects_;
  while (current_object != nullptr) {
    const auto header = PtrOfHeaderFromLarge(current_object);
    const auto next = current_object->next;
    if (!header->marked) {
      if (prev != nullptr) {
        prev->next = next;
      } else {
        large_objects_ = next;
      }
      std::free(current_object);
    } else {
      header->marked = false;
      prev = current_object;
      alive_bytes_ += current_object->size;
    }
    current_object = next;
  }

  collect_threshold_ = std::max(alive_bytes_ * 2, kInitialGcThreshold);
}
void MarkSweepGC::OnAllocation(std::size_t size) {
  allocated_bytes_ += size;
  if (allocated_bytes_ > collect_threshold_) {
    collect_requested_ = true;
  }
}
MarkSweepGC::MarkSweepGC()
    : large_objects_(nullptr),
      collect_requested_(false),
      collect_threshold_(kInitialGcThreshold),
      allocated_bytes_(0),
      alive_bytes_(0) {
  alloc_slabs_ = static_cast<AllocSlab*>(std::aligned_alloc(
      alignof(AllocSlab), kSlabSizes.size() * sizeof(AllocSlab)));
  for (auto n = 0; n < kSlabSizes.size(); ++n) {
    new (alloc_slabs_ + n) AllocSlab(kSlabSizes[n]);
  }
}
MarkSweepGC::~MarkSweepGC() {
  for (auto n = 0; n < kSlabSizes.size(); ++n) {
    alloc_slabs_[n].~AllocSlab();
  }
  std::free(alloc_slabs_);
}
AllocSlab* MarkSweepGC::FindSlab(const std::size_t size) const {
  const auto aligned = (size + kSlabAlign - 1) & ~(kSlabAlign - 1);
  return &alloc_slabs_[32 - __builtin_clz(aligned - 1) - kLog2MinSize];
}
MarkSweepGC::ObjectHeader* MarkSweepGC::HeaderOf(GCPtr<Object> ptr) {
  return reinterpret_cast<ObjectHeader*>(reinterpret_cast<char*>(ptr) -
                                         sizeof(ObjectHeader));
}
void* MarkSweepGC::PtrOfObjectFromHeader(ObjectHeader* header) {
  return reinterpret_cast<char*>(header) + sizeof(ObjectHeader);
}
void* MarkSweepGC::PtrOfObjectFromLarge(LargeObjectHeader* header) {
  return reinterpret_cast<char*>(header) + header->obj_header_offset +
         sizeof(ObjectHeader);
}
MarkSweepGC::ObjectHeader* MarkSweepGC::PtrOfHeaderFromLarge(
    LargeObjectHeader* header) {
  return reinterpret_cast<ObjectHeader*>(reinterpret_cast<char*>(header) +
                                         header->obj_header_offset);
}
bool MarkSweepGCVisitor::Visit(const Handle<Object> value) {
  const auto header = MarkSweepGC::HeaderOf(value.GetPtr());
  if (header->marked) {
    return true;
  }
  header->marked = true;
  return false;
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
