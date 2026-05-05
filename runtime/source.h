//
// Created by nothingbutyou on 5/2/26.
//

#ifndef WERSALKALANG_SOURCE_H
#define WERSALKALANG_SOURCE_H

namespace wersalka {
namespace lang {
namespace runtime {
struct TextSpan {
  TextSpan() : offset(0), length(0) {}

  TextSpan(const int offset, const int length)
      : offset(offset), length(length) {}

  static TextSpan Merge(TextSpan left, TextSpan right);

  int end() const { return offset + length; }

  int offset;
  int length;
};
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_SOURCE_H
