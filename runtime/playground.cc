//
// Created by nothingbutyou on 5/3/26.
//

#include <iostream>

#include "codegen.h"
#include "runtime/diagnostic.h"
#include "runtime/parser.h"
#include "runtime/tokenizer.h"

namespace wersalka {
namespace lang {
namespace runtime {

int Main() {
  const auto source = R"(
func main() {
  var a = 1 + (2 * 3);

  if (a > 5) {
    print(1);
  } else {
    print(2);
  }

  test(1000);

  return 1337;
}

func test(p) {
  var a = 0;
  a /= 10;
}
)";
  Zone zone;
  DiagnosticReporter reporter;
  Tokenizer tokenizer(&zone, &reporter, source);
  Parser parser(&zone, &tokenizer, &reporter);

  const auto ast = parser.Parse();

  std::cout << DumpAST(ast);

  Runtime runtime(&zone);
  CodeGenerator codegen(&runtime, &reporter);

  if (Is<ASTCompileUnit>(ast)) {
    const auto compile_unit = Cast<ASTCompileUnit>(ast);
    for (const auto function : compile_unit->functions) {
      const auto object = codegen.CreateCodeObject(function);
      std::cout << object->instructions.size() << "\n";
    }
  }

  return 0;
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

int main() { wersalka::lang::runtime::Main(); }
