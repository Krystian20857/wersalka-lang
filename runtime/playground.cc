//
// Created by nothingbutyou on 5/3/26.
//

#include <iostream>

#include "runtime/diagnostic.h"
#include "runtime/tokenizer.h"

namespace wersalka {
namespace lang {
namespace runtime {

int Main() {
  Zone zone;
  DiagnosticReporter reporter;
  Tokenizer tokenizer(&zone, &reporter, R"( -1000 )");
  std::vector<Token> tokens;
  while (true) {
    const auto& current = tokenizer.current();
    if (current.kind == TokenKind::kEnd) {
      break;
    }
    if (current.kind != TokenKind::kBegin) {
      tokens.push_back(current);
    }
    tokenizer.Next();
  }

  for (auto token : tokens) {
    std::cout << GetTokenMnemonic(token.kind) << "\n";
  }

  return 0;
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

int main() { wersalka::lang::runtime::Main(); }
