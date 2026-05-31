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
  while (true) {
    var obj = new {};
    obj.x = 1;
    obj.y = 2;
    obj.z = 3;

    var stats = __gc_stats();

    print("""Allocated {cast(stats.allocated_bytes, "float") / bytes_to_mb}mb
    Alive {cast(stats.alive_bytes, "float") / bytes_to_mb}mb""");
  }

  print("x, y, z = {obj.x}, {obj.y}, {obj.z}");
}

)";
  Zone zone;
  DiagnosticReporter reporter;
  Tokenizer tokenizer(&zone, &reporter, source);
  Parser parser(&zone, &tokenizer, &reporter);

  const auto ast = parser.Parse();

  std::cout << DumpAST(ast);

  Runtime runtime(&zone);
  runtime.RegisterBuiltIns();
  runtime.BindGlobalFunction(
      "print", 1, [](const auto context, const auto args) {
        const auto arg0 = args[0];
        std::cout << VMIntrinsics::ToString(context->runtime, arg0) << "\n";
        return Value::CreateNull();
      });
  CodeGenerator codegen(&runtime, &reporter);

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
      std::cout << absl::StrFormat("\t[%d:%d] %s\n", label.span.offset,
                                   label.span.offset + label.span.length,
                                   label.message);
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
