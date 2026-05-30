//
// Created by nothingbutyou on 5/1/26.
//

#include "zone.h"
namespace wersalka {
namespace lang {
namespace runtime {
ZoneStr Zone::InternString(const std::string_view view) {
  if (const auto interned = string_pool_.find(view);
      interned != string_pool_.end()) {
    return interned->second;
  }
  const auto buffer = NewBuffer<char>(view.size() * sizeof(char));
  std::memcpy(buffer, view.data(), view.size());
  const auto new_str = std::string_view{buffer, view.size()};
  string_pool_[new_str] = new_str;
  // ReSharper disable once CppDFALocalValueEscapesFunction
  return new_str;
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
