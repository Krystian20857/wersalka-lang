//
// Created by nothingbutyou on 5/1/26.
//

#include "runtime/driver.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "absl/strings/str_format.h"
#include "runtime/codegen.h"
#include "runtime/diagnostic.h"
#include "runtime/parser.h"
#include "runtime/source.h"
#include "runtime/tokenizer.h"
#include "runtime/vm/vm.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

static void EmitDiagnostics(const DiagnosticReporter& reporter) {
  for (const auto& diagnostic : reporter.diagnostics()) {
    const char* severity =
        (diagnostic.severity == Severity::kError) ? "error" : "warning";
    std::cerr << absl::StrFormat("[%s] %s\n", severity, diagnostic.message);
    for (const auto& label : diagnostic.labels) {
      const auto location =
          diagnostic.source_file->LocationOf(label.span.offset);
      std::cerr << absl::StrFormat("  --> [%d:%d] %s\n", location.line,
                                   location.column, label.message);
    }
    for (const auto& note : diagnostic.notes) {
      const char* kind = (note.severity == Severity::kHelp) ? "help" : "note";
      std::cerr << absl::StrFormat("  = %s: %s\n", kind, note.message);
    }
  }
}

int Driver::RunFiles(std::vector<std::string_view> files) {
  if (files.empty()) {
    std::cerr << "error: no input files\n";
    return 1;
  }

  Zone zone;
  DiagnosticReporter reporter;
  std::vector<std::unique_ptr<SourceFile>> source_files;
  source_files.reserve(files.size());

  Runtime runtime(&zone);
  runtime.builtins()->RegisterBuiltIns();
  runtime.BindGlobalFunction(
      "print", 1, [](NativeContext* context, std::span<const Value> args) {
        std::cout << VMIntrinsics::ToString(context->runtime, args[0]) << "\n";
        return Value::CreateNull();
      });

  for (const auto file_path : files) {
    std::ifstream file_stream{std::string(file_path)};
    if (!file_stream) {
      std::cerr << absl::StrFormat("error: cannot open '%s'\n", file_path);
      return 1;
    }
    std::ostringstream source_stream;
    source_stream << file_stream.rdbuf();

    auto source_file = std::make_unique<SourceFile>(source_stream.str());
    auto* source_file_ptr = source_file.get();
    source_files.push_back(std::move(source_file));

    Tokenizer tokenizer(&zone, &reporter, source_file_ptr);
    Parser parser(&zone, &tokenizer, &reporter, source_file_ptr);
    const auto ast = parser.Parse();
    if (reporter.HasError()) {
      EmitDiagnostics(reporter);
      return 1;
    }

    if (Is<ASTCompileUnit>(ast)) {
      for (const auto function : Cast<ASTCompileUnit>(ast)->functions) {
        Zone codegen_zone;
        CodeGenerator code_generator(&runtime, &reporter, &codegen_zone,
                                     source_file_ptr);
        const auto code_object = code_generator.CreateCodeObject(function);
        if (reporter.HasError()) {
          EmitDiagnostics(reporter);
          return 1;
        }
        const auto function_object = runtime.gc()->New<FunctionObject>(
            runtime.GetPermanentZone()->InternString(function->name),
            code_object);
        runtime.RegisterFunction(function_object);
      }
    }
  }

  const auto main_function = runtime.LookupFunction("main");
  if (!main_function.has_value()) {
    std::cerr << "error: no 'main' function defined\n";
    return 1;
  }

  VMInterpreter interpreter(&runtime);
  auto main_thread = std::make_unique<VMThread>(&runtime);
  interpreter.Execute(main_thread.get(), *main_function);

  if (main_thread->GetThreadState() == VMThreadState::kError) {
    std::cerr << "runtime error: "
              << VMIntrinsics::ToString(&runtime,
                                        main_thread->GetCurrentException())
              << "\n";
    return 1;
  }
  return 0;
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
