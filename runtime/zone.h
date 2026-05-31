//
// Created by nothingbutyou on 5/1/26.
//

#ifndef WERSALKALANG_ZONE_H
#define WERSALKALANG_ZONE_H

#include <memory_resource>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"

namespace wersalka {
namespace lang {
namespace runtime {

using ZoneStr = std::string_view;

class Zone {
 public:
  template <typename T, typename... Args>
  T* New(Args&&... args);

  template <typename T>
  T* NewBuffer(std::size_t size);

  ZoneStr InternString(std::string_view view);

  template <typename T>
  std::span<T> CopyArray(std::span<const T> span);

  template <typename T>
  T* InternReference(const T& ref);

 private:
  static constexpr auto kInitialBufferSize = 64 * 1024;

  std::pmr::monotonic_buffer_resource buffer_{kInitialBufferSize};
  absl::flat_hash_map<ZoneStr, ZoneStr> string_pool_;
};

// this looks a little bit too hacky,
// but at least I cannot simply `new ...` ZoneObject
class ZoneObject {
 public:
  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
  void operator delete(void*) = delete;
  void operator delete[](void*) = delete;

 protected:
  ZoneObject() = default;
  ~ZoneObject() = default;

 private:
  friend class Zone;

  void* operator new(size_t, void* ptr) noexcept { return ptr; }
};

template <typename T>
using ZonePtr = T*;

template <typename T>
class ZoneList {
 public:
  static_assert(std::is_trivially_destructible_v<T>,
                "Elements of `ZoneList` have to be trivially destructible");

  using const_iterator = const T*;

  explicit ZoneList(Zone* zone) : ZoneList(zone, kDefaultCapacity) {}
  ZoneList(Zone* zone, int capacity);

  T& operator[](int idx) const { return elements_[idx]; }
  const_iterator begin() const { return &elements_[0]; }
  const_iterator end() const { return &elements_[size_]; }
  int size() const { return size_; }
  int empty() const { return size_ == 0; }

  template <typename Elem>
  void Add(Zone* zone, Elem&& element);
  void Clear();

  T& Back() const;
  void PopBack();
  void Reserve(Zone* zone, int size);

  std::vector<T> ToVector();
  std::span<const T> ToSpan() const;

 private:
  static constexpr auto kDefaultCapacity = 16;
  static constexpr auto kGrowFactor = 2;

  T* elements_;
  int capacity_;
  int size_;
};

template <typename T>
using ZonePtrList = ZoneList<ZonePtr<T>>;

// Zone - Impl

template <typename T, typename... Args>
T* Zone::New(Args&&... args) {
  static_assert(std::is_trivially_destructible_v<T>,
                "Usage of `Zone.New` require trivially destructible type");
  const auto ptr = buffer_.allocate(sizeof(T), alignof(T));
  return new (ptr) T(std::forward<Args>(args)...);
}

template <typename T>
T* Zone::NewBuffer(const std::size_t size) {
  return static_cast<T*>(buffer_.allocate(size * sizeof(T), alignof(T)));
}

template <typename T>
std::span<T> Zone::CopyArray(std::span<const T> span) {
  static_assert(
      std::is_trivially_destructible_v<T>,
      "Usage of `Zone.CopyArray` require trivially destructible type");
  if (span.empty()) {
    return std::span<T>{};
  }

  const auto ptr = NewBuffer<T>(span.size());
  std::uninitialized_copy(span.begin(), span.end(), ptr);
  return std::span{ptr, span.size()};
}

template <typename T>
T* Zone::InternReference(const T& ref) {
  static_assert(
      std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>,
      "Usage of `Zone.InternReference` require trivially "
      "copyable and trivially destructible type");
  const auto ptr = buffer_.allocate(sizeof(T), alignof(T));
  std::memcpy(ptr, &ref, sizeof(T));
  return static_cast<T*>(ptr);
}

// ZoneList - Impl

template <typename T>
ZoneList<T>::ZoneList(Zone* zone, const int capacity)
    : capacity_(capacity), size_(0) {
  CHECK_GE(capacity, 0);
  if (capacity > 0) {
    elements_ = zone->NewBuffer<T>(capacity);
  } else {
    elements_ = nullptr;
  }
}

template <typename T>
template <typename Elem>
void ZoneList<T>::Add(Zone* zone, Elem&& element) {
  CHECK_NE(zone, nullptr);
  if (capacity_ == size_ || elements_ == nullptr) {
    const auto new_capacity = std::max(capacity_, 1) * kGrowFactor;
    auto* new_elements = zone->NewBuffer<T>(new_capacity);
    std::memcpy(new_elements, elements_, capacity_ * sizeof(T));
    capacity_ = new_capacity;
    elements_ = new_elements;
  }
  elements_[size_++] = std::forward<Elem>(element);
}
template <typename T>
void ZoneList<T>::Clear() {
  size_ = 0;
}
template <typename T>
T& ZoneList<T>::Back() const {
  CHECK(size_ >= 1);
  return elements_[size_ - 1];
}
template <typename T>
void ZoneList<T>::PopBack() {
  size_--;
}
template <typename T>
void ZoneList<T>::Reserve(Zone* zone, int size) {
  if (elements_ == nullptr) {
    elements_ = zone->NewBuffer<T>(size);
    capacity_ = size;
  } else {
    auto* new_elements = zone->NewBuffer<T>(size);
    std::memcpy(new_elements, elements_, capacity_ * sizeof(T));
    capacity_ = size;
    elements_ = new_elements;
  }
}

template <typename T>
std::vector<T> ZoneList<T>::ToVector() {
  return std::vector<T>{begin(), end()};
}
template <typename T>
std::span<const T> ZoneList<T>::ToSpan() const {
  return std::span{elements_, static_cast<std::size_t>(size_)};
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_ZONE_H
