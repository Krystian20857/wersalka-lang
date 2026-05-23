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

  std::string Format() const;

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

  ZonePtr<ASTNode> Parse(bool expr = false);

 private:
  enum class Precedence {
    kNone,
    kAssignment,  // right-assoc
    kLogicOr,
    kLogicAnd,
    kBitwiseOr,
    kBitwiseXor,
    kBitwiseAnd,
    kEquality,
    kComparison,
    kShift,
    kAdditive,
    kMultiplicative,
    kExponent,  // right-assoc
    kPostfix,
  };

  static constexpr auto kTokenBufferSize = 512;

  ZonePtr<ASTFunctionDecl> ParseFuncDecl();

  ZonePtr<ASTExpr> ParseExpr(Precedence precedence);
  ZonePtr<ASTExpr> ParsePrimaryExpr();
  ZonePtr<ASTExpr> ParseGroupExpr();
  ZonePtr<ASTExpr> ParseBinarExpr(ZonePtr<ASTExpr> left, ZonePtr<Token> op_token);
  ZonePtr<ASTExpr> ParseAssignExpr(ZonePtr<ASTExpr> target, ZonePtr<Token> op_token, ASTAssignExpr::Operator op);
  ZonePtr<ASTExpr> ParseUnaryExpr();
  ZonePtr<ASTExpr> ParseMemberExpr();
  ZonePtr<ASTExpr> ParseCallExpr(ZonePtr<ASTExpr> left);
  ZonePtr<ASTExpr> ParseTemplateExpr();

  ZonePtr<ASTStmt> ParseStmt();
  ZonePtr<ASTStmt> ParseBlockStmt();
  ZonePtr<ASTStmt> ParseVarStmt();
  ZonePtr<ASTStmt> ParseIfStmt();
  ZonePtr<ASTStmt> ParseWhileStmt();
  ZonePtr<ASTStmt> ParseExprStmt();
  ZonePtr<ASTStmt> ParseReturnStmt();

  ZonePtr<Token> Peek(int offset = 0);
  ZonePtr<Token> Next();
  bool At(const TokenSet& tokens);
  bool At(const TokenKind kind) { return At(TokenSet(kind)); }
  bool AtEnd() { return At(TokenKind::kEnd); }
  bool TryConsume(const TokenSet& tokens);
  bool TryConsume(const TokenKind kind) { return TryConsume(TokenSet(kind)); }
  ZonePtr<Token> Expect(const TokenSet& tokens);
  ZonePtr<Token> Expect(const TokenKind kind) { return Expect(TokenSet(kind)); }
  void Synchronize();
  int SpanBegin() const ;
  TextSpan SpanEnd(int mark) const;

  static Precedence GetPrecedence(TokenKind kind);

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
