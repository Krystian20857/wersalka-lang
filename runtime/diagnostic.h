//
// Created by nothingbutyou on 5/1/26.
//

#ifndef WERSALKALANG_ERROR_H
#define WERSALKALANG_ERROR_H

#include <string>
#include <vector>

#include "runtime/source.h"

namespace wersalka {
namespace lang {
namespace runtime {

enum class Severity { kError, kWarn, kNote, kHelp };

struct Label {
  TextSpan span;
  std::string message;
};

struct Diagnostic {
  Diagnostic(const Severity severity, const std::string& message)
      : severity(severity), message(message) {}

  static Diagnostic Error(const std::string& message) {
    return Diagnostic(Severity::kError, message);
  }
  static Diagnostic Warn(const std::string& message) {
    return Diagnostic(Severity::kWarn, message);
  }

  Diagnostic& withLabel(TextSpan span, std::string msg) {
    labels.emplace_back(span, msg);
    return *this;
  }

  Diagnostic& withNote(std::string msg) {
    notes.emplace_back(Severity::kNote, msg);
    return *this;
  }

  Diagnostic& withHelp(std::string msg) {
    notes.emplace_back(Severity::kHelp, msg);
    return *this;
  }

  Severity severity;
  std::string message;
  std::vector<Label> labels;
  std::vector<Diagnostic> notes;
};

class DiagnosticReporter {
 public:
  void Report(const Diagnostic& diagnostic);
  bool HasError() const { return error_count_ > 0; }
  const std::vector<Diagnostic>& diagnostics() const{ return diagnostics_; }

 private:
  std::vector<Diagnostic> diagnostics_;
  int error_count_ = 0;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_ERROR_H
