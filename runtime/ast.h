//
// Created by nothingbutyou on 5/1/26.
//

#ifndef WERSALKALANG_AST_H
#define WERSALKALANG_AST_H

#include "runtime/tokenizer.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

class ASTNode : public ZoneObject {
 public:
  enum class Kind {
    kUnknown = 0,

    // skeleton
    kCompileUnit,

    // expressions
    kConstExpr,
    kBinaryExpr,
    kUnaryExpr,
    kGroupExpr
  };

  static constexpr auto kKind = Kind::kUnknown;

  explicit ASTNode(const Kind kind, const TextSpan span)
      : kind_(kind), span_(span) {}

  Kind kind() const { return kind_; }
  TextSpan span() const { return span_; }

 private:
  Kind kind_;
  TextSpan span_;
};

// skeleton

struct ASTCompileUnit : ASTNode {
  explicit ASTCompileUnit(const TextSpan span)
      : ASTNode(Kind::kCompileUnit, span) {}
};

// expressions

struct ASTExpr : ASTNode {
  explicit ASTExpr(const Kind kind, const TextSpan span)
      : ASTNode(kind, span) {}
};

struct ASTConstExpr : ASTExpr {
  explicit ASTConstExpr(const Token& value, const TextSpan span)
      : ASTExpr(Kind::kConstExpr, span), value(value) {}

  const Token& value;
};

struct ASTBinaryExpr : ASTExpr {
  enum class Operator {
    // arithmetic
    kAdd,
    kSub,
    kMul,
    kDiv,
    kMod,

    // bitwise
    kBitwiseAnd,
    kBitwiseOr,
    kBitwiseXor,
    kBitwiseShl,
    kBitwiseShr,

    // logical
    kLogicAnd,
    kLogicOr,

    // compare
    kCmpGt,
    kCmpLt,
    kCmpGe,
    kCmpLe,
    kCmpEq,
    kCmpNe
  };

  explicit ASTBinaryExpr(const TextSpan span, const ZonePtr<ASTExpr> left,
                         const ZonePtr<ASTExpr> right, const Operator op)
      : ASTExpr(Kind::kBinaryExpr, span), left(left), right(right), op(op) {}

  ZonePtr<ASTExpr> left;
  ZonePtr<ASTExpr> right;
  Operator op;
};

struct ASTUnaryExpr : ASTExpr {
  enum class Operator { kPlus, kLogicNeg, kLogic };

  ASTUnaryExpr(const TextSpan& span, const ZonePtr<ASTExpr> expr,
               const Operator op)
      : ASTExpr(Kind::kUnaryExpr, span), expr(expr), op(op) {}

  ZonePtr<ASTExpr> expr;
  Operator op;
};

struct ASTGroupExpr : ASTExpr {
  explicit ASTGroupExpr(const TextSpan span, const ZonePtr<ASTExpr> expr)
      : ASTExpr(Kind::kGroupExpr, span), expr(expr) {}

  ZonePtr<ASTExpr> expr;
};

// struct ASTTemplateExpr : ASTExpr {};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_AST_H
