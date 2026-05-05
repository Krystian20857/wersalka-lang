//
// Created by nothingbutyou on 5/1/26.
//

#include "runtime/parser.h"

namespace wersalka {
namespace lang {
namespace runtime {
namespace kIdent {
constexpr auto kSyncTokens =
    TokenSet { TokenKind::kOpenBrace, TokenKind::kCloseBrace,
             TokenKind::kOpenParen, TokenKind::kCloseParen, TokenKind::kSemi };
}
ZonePtr<ASTNode> Parser::Parse() {}
ZonePtr<Token> Parser::Peek(int offset) {
  while (pos_ + offset >= tokens_.size()) {
    tokenizer_->Next();
    const auto& current = tokenizer_->current();
    if (current.kind == TokenKind::kEnd) {
      return zone_->InternReference(current);
    }
    tokens_.Add(zone_, zone_->InternReference(current));
  }
  return tokens_[pos_ + offset];
}
ZonePtr<Token> Parser::Next() { pos_++; }
bool Parser::At(const TokenSet& tokens) {
  return tokens.Contains(Peek()->kind);
}
bool Parser::TryConsume(const TokenSet& tokens) {
  if (At(tokens)) {
    Next();
    return true;
  }
  return false;
}
bool Parser::Expect(const TokenSet& tokens) {
  if (At(tokens)) {
    Next();
    return true;
  }
  return false;
}
void Parser::Synchronize() {}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
