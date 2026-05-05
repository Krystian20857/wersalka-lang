//
// Created by nothingbutyou on 5/1/26.
//

#ifndef WERSALKALANG_ZONE_H
#define WERSALKALANG_ZONE_H

#include <memory_resource>

#include "absl/log/check.h"

namespace wersalka {
namespace lang {
namespace runtime {

class Zone {
 public:
  template <typename T, typename... Args>
  T* New(Args&&... args);

  template <typename T>
  T* NewBuffer(std::size_t size);

  std::string_view InternString(std::string_view view);

  template <typename T>
  T* InternReference(const T& ref);

 private:
  static constexpr auto kInitialBufferSize = 64 * 1024;

  std::pmr::monotonic_buffer_resource buffer_{kInitialBufferSize};
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

  ZoneList(Zone* zone, int capacity);

  T& operator[](int idx) const { return elements_[idx]; }
  const_iterator begin() const { return &elements_[0]; }
  const_iterator end() const { return &elements_[size_]; }
  int size() const { return size_; }

  template <typename Elem>
  void Add(Zone* zone, Elem&& element);

  std::vector<T> ToVector();

 private:
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
  return new (ptr) T{std::forward<Args>(args)...};
}

template <typename T>
T* Zone::NewBuffer(const std::size_t size) {
  return static_cast<T*>(buffer_.allocate(size * sizeof(T), alignof(T)));
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
    std::memcpy(new_elements, elements_, capacity_);
    capacity_ = new_capacity;
    elements_ = new_elements;
  }
  elements_[size_++] = std::forward<Elem>(element);
}

template <typename T>
std::vector<T> ZoneList<T>::ToVector() {
  return std::vector<T>{begin(), end()};
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_ZONE_H
