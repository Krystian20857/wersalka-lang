//
// Created by nothingbutyou on 5/1/26.
//

#ifndef WERSALKALANG_PARSER_H
#define WERSALKALANG_PARSER_H
#include "runtime/ast.h"
#include "runtime/tokenizer.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

class TokenSet final {
 public:
  constexpr explicit TokenSet(std::initializer_list<TokenKind> tokens);
  constexpr explicit TokenSet(TokenKind token) : TokenSet({token}) {}

  constexpr friend TokenSet operator|(const TokenSet& left,
                                      const TokenSet& right) {
    return TokenSet(left.bitset_ | right.bitset_);
  }

  constexpr bool Contains(TokenKind kind) const;

 private:
  using TokenBitSet =
      std::bitset<static_cast<std::size_t>(TokenKind::kReserved)>;

  constexpr explicit TokenSet(const TokenBitSet&& bitset) : bitset_(bitset) {}

  TokenBitSet bitset_;
};

class Parser final {
 public:
  explicit Parser(Zone* zone, Tokenizer* tokenizer,
                  DiagnosticReporter* reporter)
      : zone_(zone),
        reporter_(reporter),
        tokenizer_(tokenizer),
        tokens_(zone, kTokenBufferSize),
        pos_(0) {}

  ZonePtr<ASTNode> Parse();

 private:
  static constexpr auto kTokenBufferSize = 128;

  ZonePtr<ASTExpr> ParseExpr();
  ZonePtr<ASTExpr> ParsePrimaryExpr();
  ZonePtr<ASTExpr> ParseBinarExpr(ZonePtr<ASTExpr> left);

  ZonePtr<Token> Peek(int offset = 0);
  ZonePtr<Token> Next();
  bool At(const TokenSet& tokens);
  bool At(const TokenKind kind) { return At(TokenSet(kind)); }
  bool AtEnd() { return At(TokenKind::kEnd); }
  bool TryConsume(const TokenSet& tokens);
  bool TryConsume(const TokenKind kind) { return At(TokenSet(kind)); }
  bool Expect(const TokenSet& tokens);
  bool Expect(const TokenKind kind) { return Expect(TokenSet(kind)); }
  void Synchronize();

  Zone* zone_;
  DiagnosticReporter* reporter_;
  Tokenizer* tokenizer_;
  ZonePtrList<Token> tokens_;
  int pos_;
};

constexpr TokenSet::TokenSet(std::initializer_list<TokenKind> tokens) {
  for (auto token : tokens) {
    bitset_.set(static_cast<std::size_t>(token));
  }
}
constexpr bool TokenSet::Contains(TokenKind kind) const {
  return bitset_.test(static_cast<std::size_t>(kind));
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_PARSER_H
