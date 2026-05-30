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
  var array = new [1, 2, 3, 4];

  array[0] = 5;
  print("length: {len(array)}");
  print("array[0] = {array[0]}");
  print("array[1] = {array[1]}");

  print("=================");
  var n = 0;
  while (n < len(array)) {
    print("array[{n}] = {array[n]}");
    n += 1;
  }
}

)";
  Zone zone;
  DiagnosticReporter reporter;
  Tokenizer tokenizer(&zone, &reporter, source);
  Parser parser(&zone, &tokenizer, &reporter);

  const auto ast = parser.Parse();

  std::cout << DumpAST(ast);

  Runtime runtime(&zone);
  runtime.BindGlobalFunction(
      "print", 1, [](const auto context, const auto args) {
        const auto arg0 = args[0];
        std::cout << VMIntrinsics::ToString(context->runtime, arg0) << "\n";
        return Value::CreateNull();
      });
  runtime.BindGlobalFunction("len", 1, [](const auto context, const auto args) {
    const auto arg0 = args[0];
    const auto length =
        arg0.template GetObjectUnchecked<ArrayObject>()->length();
    return Value::CreateInt(length);
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
