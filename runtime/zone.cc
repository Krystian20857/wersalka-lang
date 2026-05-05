//
// Created by nothingbutyou on 5/1/26.
//

#include "zone.h"
namespace wersalka {
namespace lang {
namespace runtime {
std::string_view Zone::InternString(const std::string_view view) {
  const auto buffer = NewBuffer<char>(view.size() * sizeof(char));
  std::memcpy(buffer, view.data(), view.size());
  // ReSharper disable once CppDFALocalValueEscapesFunction
  return {buffer, view.size()};
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
