//
// Created by nothingbutyou on 5/1/26.
//

#include "runtime/diagnostic.h"
namespace wersalka {
namespace lang {
namespace runtime {
void DiagnosticReporter::Report(const Diagnostic& diagnostic) {
  if (diagnostic.severity == Severity::kError) {
    error_count_++;
  }
  diagnostics_.push_back(diagnostic);
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
