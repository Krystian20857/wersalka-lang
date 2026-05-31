//
// Created by nothingbutyou on 5/2/26.
//

#ifndef WERSALKALANG_SOURCE_H
#define WERSALKALANG_SOURCE_H

#include <string>
#include <vector>

#include "runtime/zone.h"

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

struct SourceLocation {
  int line;
  int column;
};

class SourceFile {
 public:
  explicit SourceFile(const std::string& source) : source_(source) {
    BuildLineTable();
  }

  const std::string& source() const { return source_; }

  SourceLocation LocationOf(int offset) const;

 private:
  void BuildLineTable();

  std::string source_;
  std::vector<int> line_starts_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_SOURCE_H
