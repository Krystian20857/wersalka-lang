//
// Created by nothingbutyou on 5/1/26.
//

#ifndef WERSALKALANG_AST_H
#define WERSALKALANG_AST_H

#include <string>

#include "ast.h"
#include "runtime/tokenizer.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

struct ASTExpr;
struct ASTStmt;
struct ASTBlockStmt;
struct ASTFunctionDecl;

class ASTNode : public ZoneObject {
 public:
  enum class Kind {
    kUnknown = 0,

    // skeleton
    kCompileUnit,
    kFunctionDecl,

    // stmts
    kBlockStmt,
    kVarStmt,
    kExprStmt,
    kIfStmt,
    kWhileStmt,
    kReturnStmt,

    // expressions
    kConstExpr,
    kTemplateExpr,
    kBinaryExpr,
    kUnaryExpr,
    kGroupExpr,
    kIdentExpr,
    kCallExpr,
    kAssignExpr,
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

template <typename T>
bool Is(const ZonePtr<ASTNode> node) {
  return node->kind() == T::kKind;
}

template <typename T>
ZonePtr<T> Cast(const ZonePtr<ASTNode> node) {
  CHECK(Is<T>(node));
  return static_cast<T*>(node);
}

// skeleton

struct ASTCompileUnit : ASTNode {
  static constexpr auto kKind = Kind::kCompileUnit;

  explicit ASTCompileUnit(const TextSpan& span,
                          const ZonePtrList<ASTFunctionDecl>& functions,
                          const ZonePtrList<ASTStmt>& stmts)
      : ASTNode(Kind::kCompileUnit, span), functions(functions), stmts(stmts) {}

  ZonePtrList<ASTFunctionDecl> functions;
  ZonePtrList<ASTStmt> stmts;
};

struct ASTFunctionDecl : ASTNode {
  static constexpr auto kKind = Kind::kFunctionDecl;

  explicit ASTFunctionDecl(const TextSpan& span, const ZoneStr& name,
                           const ZoneList<ZoneStr>& params,
                           ZonePtr<ASTStmt> block)
      : ASTNode(Kind::kFunctionDecl, span),
        name(name),
        params(params),
        block(block) {}

  ZoneStr name;
  ZoneList<ZoneStr> params;
  ZonePtr<ASTStmt> block;
};

// stmts

struct ASTStmt : ASTNode {
  explicit ASTStmt(const Kind kind, const TextSpan span)
      : ASTNode(kind, span) {}
};

struct ASTBlockStmt : ASTStmt {
  static constexpr auto kKind = Kind::kBlockStmt;

  explicit ASTBlockStmt(const TextSpan& span, const ZonePtrList<ASTStmt> stmts)
      : ASTStmt(Kind::kBlockStmt, span), stmts(stmts) {}

  ZonePtrList<ASTStmt> stmts;
};

struct ASTVarStmt : ASTStmt {
  static constexpr auto kKind = Kind::kVarStmt;

  explicit ASTVarStmt(const TextSpan& span, const ZoneStr name,
                      const std::optional<ZonePtr<ASTExpr>> init_expr)
      : ASTStmt(Kind::kVarStmt, span), name(name), init_expr(init_expr) {}
  ZoneStr name;
  std::optional<ZonePtr<ASTExpr>> init_expr;
};

struct ASTExprStmt : ASTStmt {
  static constexpr auto kKind = Kind::kExprStmt;

  explicit ASTExprStmt(const TextSpan& span, const ZonePtr<ASTExpr> expr)
      : ASTStmt(Kind::kExprStmt, span), expr(expr) {}
  ZonePtr<ASTExpr> expr;
};

struct ASTIfStmt : ASTStmt {
  static constexpr auto kKind = Kind::kIfStmt;

  explicit ASTIfStmt(const TextSpan& span, const ZonePtr<ASTExpr> condition,
                     const ZonePtr<ASTStmt> then,
                     const std::optional<ZonePtr<ASTIfStmt>> alternate,
                     const std::optional<ZonePtr<ASTStmt>> else_block)
      : ASTStmt(Kind::kIfStmt, span),
        condition(condition),
        then(then),
        else_block(else_block),
        alternate(alternate) {}

  ZonePtr<ASTExpr> condition;
  ZonePtr<ASTStmt> then;
  std::optional<ZonePtr<ASTStmt>> else_block;
  std::optional<ZonePtr<ASTIfStmt>> alternate;
};

struct ASTWhileStmt : ASTStmt {
  static constexpr auto kKind = Kind::kWhileStmt;

  explicit ASTWhileStmt(const TextSpan& span, const ZonePtr<ASTExpr> condition,
                        const ZonePtr<ASTStmt> block)
      : ASTStmt(Kind::kWhileStmt, span), condition(condition), block(block) {}
  ZonePtr<ASTExpr> condition;
  ZonePtr<ASTStmt> block;
};

struct ASTReturnStmt : ASTStmt {
  static constexpr auto kKind = Kind::kReturnStmt;

  explicit ASTReturnStmt(const TextSpan& span, const ZonePtr<ASTExpr> expr)
      : ASTStmt(Kind::kReturnStmt, span), expr(expr) {}

  ZonePtr<ASTExpr> expr;
};

// expressions

struct ASTExpr : ASTNode {
  explicit ASTExpr(const Kind kind, const TextSpan span)
      : ASTNode(kind, span) {}
};

struct ASTConstExpr : ASTExpr {
  static constexpr auto kKind = Kind::kConstExpr;

  explicit ASTConstExpr(const ZonePtr<Token> value, const TextSpan span)
      : ASTExpr(Kind::kConstExpr, span), value(value) {}

  const ZonePtr<Token> value;
};

struct ASTTemplateExpr : ASTExpr {
  static constexpr auto kKind = Kind::kTemplateExpr;

  struct Segment {
    enum Kind { kPart, kExpr };

    Kind kind;
    union {
      ZoneStr str_v;
      ZonePtr<ASTExpr> expr_v;
    };
  };

  explicit ASTTemplateExpr(const TextSpan span, const ZoneList<Segment> segments)
      : ASTExpr(Kind::kTemplateExpr, span), segments(segments) {}

  ZoneList<Segment> segments;
};

struct ASTBinaryExpr : ASTExpr {
  static constexpr auto kKind = Kind::kBinaryExpr;

  enum class Operator {
    // arithmetic
    kAdd,
    kSub,
    kMul,
    kDiv,
    kMod,
    kExp,

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
  static constexpr auto kKind = Kind::kUnaryExpr;

  enum class Operator { kPlus, kNeg, kLogicNeg, kBitwiseNeg };

  explicit ASTUnaryExpr(const TextSpan& span, const ZonePtr<ASTExpr> expr,
                        const Operator op)
      : ASTExpr(Kind::kUnaryExpr, span), expr(expr), op(op) {}

  ZonePtr<ASTExpr> expr;
  Operator op;
};

struct ASTGroupExpr : ASTExpr {
  static constexpr auto kKind = Kind::kGroupExpr;

  explicit ASTGroupExpr(const TextSpan span, const ZonePtr<ASTExpr> expr)
      : ASTExpr(Kind::kGroupExpr, span), expr(expr) {}

  ZonePtr<ASTExpr> expr;
};

struct ASTIdentExpr : ASTExpr {
  static constexpr auto kKind = Kind::kIdentExpr;

  explicit ASTIdentExpr(const TextSpan span, const ZoneStr ident)
      : ASTExpr(Kind::kIdentExpr, span), ident(ident) {}

  ZoneStr ident;
};

struct ASTAssignExpr : ASTExpr {
  static constexpr auto kKind = Kind::kAssignExpr;

  enum class Operator {
    kAssign,
    kAddAssign,
    kSubAssign,
    kMulAssign,
    kDivAssign,
    kModAssign,
    kAndAssign,
    kOrAssign,
    kXorAssign,
    kShlAssign,
    kShrAssign,
  };

  explicit ASTAssignExpr(const TextSpan span, const ZonePtr<ASTExpr> target,
                         const ZonePtr<ASTExpr> value, const Operator op)
      : ASTExpr(Kind::kAssignExpr, span),
        target(target),
        value(value),
        op(op) {}

  ZonePtr<ASTExpr> target;
  ZonePtr<ASTExpr> value;
  Operator op;
};

struct ASTCallExpr : ASTExpr {
  static constexpr auto kKind = Kind::kCallExpr;

  explicit ASTCallExpr(const TextSpan& span, const ZonePtr<ASTExpr>& callee,
                       const ZonePtrList<ASTExpr>& args)
      : ASTExpr(Kind::kCallExpr, span), callee(callee), args(args) {}
  ZonePtr<ASTExpr> callee;
  ZonePtrList<ASTExpr> args;
};

// struct ASTTemplateExpr : ASTExpr {};

std::string_view GetBinaryOpMnemonic(ASTBinaryExpr::Operator op);
std::string_view GetUnaryOpMnemonic(ASTUnaryExpr::Operator op);
std::string_view GetAssignOpMnemonic(ASTAssignExpr::Operator op);
std::string DumpAST(const ASTNode* node);

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_AST_H
