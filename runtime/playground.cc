//
// Created by nothingbutyou on 5/3/26.
//

#include <iostream>

#include "absl/strings/str_format.h"
#include "runtime/codegen.h"
#include "runtime/diagnostic.h"
#include "runtime/parser.h"
#include "runtime/tokenizer.h"
#include "runtime/vm/vm.h"

namespace wersalka {
namespace lang {
namespace runtime {

int Main() {
  const auto source = R"(

func main() {
  var bytes_to_mb = 1024 * 1024;
  var n = 0;
  while (true) {
    var obj = new {};
    obj.x = 1;
    obj.y = 2;
    obj.z = 3;

    var array = new [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

    if (n % 100000 == 0) {
      var stats = __gc_stats();
      print("""Allocated {cast(stats.allocated_bytes, "float") / bytes_to_mb}mb
      Alive {cast(stats.alive_bytes, "float") / bytes_to_mb}mb""");
    }
    n += 1;
  }

  print("x, y, z = {obj.x}, {obj.y}, {obj.z}");
}

)";
  Zone zone;
  DiagnosticReporter reporter;

  const auto source_file = std::make_unique<SourceFile>(source);
  Tokenizer tokenizer(&zone, &reporter, source_file.get());
  Parser parser(&zone, &tokenizer, &reporter, source_file.get());

  const auto ast = parser.Parse();

  std::cout << DumpAST(ast);

  Runtime runtime(&zone);
  runtime.builtins()->RegisterBuiltIns();
  runtime.BindGlobalFunction(
      "print", 1, [](const auto context, const auto args) {
        const auto arg0 = args[0];
        std::cout << VMIntrinsics::ToString(context->runtime, arg0)
                  << std::endl;
        return Value::CreateNull();
      });
  Zone codegen_zone;
  CodeGenerator codegen(&runtime, &reporter, &codegen_zone, source_file.get());

  if (Is<ASTCompileUnit>(ast)) {
    const auto compile_unit = Cast<ASTCompileUnit>(ast);
    for (const auto function : compile_unit->functions) {
      const auto object = codegen.CreateCodeObject(function);
      std::cout << absl::StrFormat("MAX_STACK %d\nMAX_LOCALS %d\n",
                                   object->max_stack, object->max_locals);
      const auto func_obj = runtime.gc()->New<FunctionObject>(
          runtime.GetPermanentZone()->InternString(function->name), object);
      runtime.RegisterFunction(func_obj);
    }
  }

  for (auto diagnostic : reporter.diagnostics()) {
    std::cout << diagnostic.message << std::endl;
    for (auto label : diagnostic.labels) {
      const auto location =
          diagnostic.source_file->LocationOf(label.span.offset);
      std::cout << absl::StrFormat("\t[%d:%d] %s\n", location.line,
                                   location.column, label.message);
    }
  }

  std::cout << std::endl;  // flush

  const auto main_func = runtime.LookupFunction("main");
  CHECK(main_func.has_value()) << "no main";

  VMInterpreter interpreter(&runtime);
  const auto main_thread = std::make_unique<VMThread>(&runtime);
  const auto value = interpreter.Execute(main_thread.get(), *main_func);
  if (value.IsNull()) {
    std::cout << "null\n";
  }

  return 0;
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

int main() { wersalka::lang::runtime::Main(); }
