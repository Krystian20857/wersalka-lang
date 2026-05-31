//
// Created by nothingbutyou on 5/2/26.
//

#include "source.h"

#include <algorithm>

namespace wersalka {
namespace lang {
namespace runtime {

void SourceFile::BuildLineTable() {
  line_starts_.push_back(0);
  for (int i = 0; i < static_cast<int>(source_.size()); ++i) {
    if (source_[i] == '\n') {
      line_starts_.push_back(i + 1);
    }
  }
}

SourceLocation SourceFile::LocationOf(const int offset) const {
  const auto it =
      std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
  const auto line = static_cast<int>(it - line_starts_.begin());
  const auto column = offset - line_starts_[line - 1] + 1;
  return {line, column};
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
