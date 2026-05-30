//
// Created by nothingbutyou on 5/1/26.
//

#include "ast.h"

#include "absl/strings/str_format.h"

namespace wersalka {
namespace lang {
namespace runtime {

std::string_view GetBinaryOpMnemonic(ASTBinaryExpr::Operator op) {
  switch (op) {
    case ASTBinaryExpr::Operator::kAdd:
      return "+";
    case ASTBinaryExpr::Operator::kSub:
      return "-";
    case ASTBinaryExpr::Operator::kMul:
      return "*";
    case ASTBinaryExpr::Operator::kDiv:
      return "/";
    case ASTBinaryExpr::Operator::kMod:
      return "%";
    case ASTBinaryExpr::Operator::kExp:
      return "**";
    case ASTBinaryExpr::Operator::kBitwiseAnd:
      return "&";
    case ASTBinaryExpr::Operator::kBitwiseOr:
      return "|";
    case ASTBinaryExpr::Operator::kBitwiseXor:
      return "^";
    case ASTBinaryExpr::Operator::kBitwiseShl:
      return "<<";
    case ASTBinaryExpr::Operator::kBitwiseShr:
      return ">>";
    case ASTBinaryExpr::Operator::kLogicAnd:
      return "&&";
    case ASTBinaryExpr::Operator::kLogicOr:
      return "||";
    case ASTBinaryExpr::Operator::kCmpEq:
      return "==";
    case ASTBinaryExpr::Operator::kCmpNe:
      return "!=";
    case ASTBinaryExpr::Operator::kCmpLt:
      return "<";
    case ASTBinaryExpr::Operator::kCmpGt:
      return ">";
    case ASTBinaryExpr::Operator::kCmpLe:
      return "<=";
    case ASTBinaryExpr::Operator::kCmpGe:
      return ">=";
  }
}

std::string_view GetAssignOpMnemonic(ASTAssignExpr::Operator op) {
  switch (op) {
    case ASTAssignExpr::Operator::kAssign:
      return "=";
    case ASTAssignExpr::Operator::kAddAssign:
      return "+=";
    case ASTAssignExpr::Operator::kSubAssign:
      return "-=";
    case ASTAssignExpr::Operator::kMulAssign:
      return "*=";
    case ASTAssignExpr::Operator::kDivAssign:
      return "/=";
    case ASTAssignExpr::Operator::kModAssign:
      return "%=";
    case ASTAssignExpr::Operator::kAndAssign:
      return "&=";
    case ASTAssignExpr::Operator::kOrAssign:
      return "|=";
    case ASTAssignExpr::Operator::kXorAssign:
      return "^=";
    case ASTAssignExpr::Operator::kShlAssign:
      return "<<=";
    case ASTAssignExpr::Operator::kShrAssign:
      return ">>=";
  }
}

std::string_view GetUnaryOpMnemonic(ASTUnaryExpr::Operator op) {
  switch (op) {
    case ASTUnaryExpr::Operator::kPlus:
      return "+";
    case ASTUnaryExpr::Operator::kNeg:
      return "-";
    case ASTUnaryExpr::Operator::kLogicNeg:
      return "!";
    case ASTUnaryExpr::Operator::kBitwiseNeg:
      return "~";
  }
}

static void DumpNode(const ASTNode* node, std::string& out, int depth) {
  const std::string indent(depth * 2, ' ');
  switch (node->kind()) {
    case ASTNode::Kind::kCompileUnit: {
      const auto* n = static_cast<const ASTCompileUnit*>(node);
      absl::StrAppendFormat(&out, "%sCompileUnit\n", indent);
      for (int i = 0; i < n->functions.size(); i++) {
        DumpNode(n->functions[i], out, depth + 1);
      }
      for (int i = 0; i < n->stmts.size(); i++) {
        DumpNode(n->stmts[i], out, depth + 1);
      }
      break;
    }
    case ASTNode::Kind::kFunctionDecl: {
      const auto* n = static_cast<const ASTFunctionDecl*>(node);
      absl::StrAppendFormat(&out, "%sFunctionDecl [%s]\n", indent, n->name);
      for (int i = 0; i < n->params.size(); i++) {
        absl::StrAppendFormat(&out, "%s  Param [%s]\n", indent, n->params[i]);
      }
      DumpNode(n->block, out, depth + 1);
      break;
    }
    case ASTNode::Kind::kBlockStmt: {
      const auto* n = static_cast<const ASTBlockStmt*>(node);
      absl::StrAppendFormat(&out, "%sBlockStmt\n", indent);
      for (int i = 0; i < n->stmts.size(); i++) {
        DumpNode(n->stmts[i], out, depth + 1);
      }
      break;
    }
    case ASTNode::Kind::kVarStmt: {
      const auto* n = static_cast<const ASTVarStmt*>(node);
      absl::StrAppendFormat(&out, "%sVarStmt [%s]\n", indent, n->name);
      if (n->init_expr.has_value()) {
        DumpNode(*n->init_expr, out, depth + 1);
      }
      break;
    }
    case ASTNode::Kind::kReturnStmt: {
      const auto* n = static_cast<const ASTReturnStmt*>(node);
      absl::StrAppendFormat(&out, "%sReturnStmt\n", indent);
      DumpNode(n->expr, out, depth + 1);
      break;
    }
    case ASTNode::Kind::kExprStmt: {
      const auto* n = static_cast<const ASTExprStmt*>(node);
      absl::StrAppendFormat(&out, "%sExprStmt\n", indent);
      DumpNode(n->expr, out, depth + 1);
      break;
    }
    case ASTNode::Kind::kIfStmt: {
      const auto* n = static_cast<const ASTIfStmt*>(node);
      absl::StrAppendFormat(&out, "%sIfStmt\n", indent);
      DumpNode(n->condition, out, depth + 1);
      DumpNode(n->then, out, depth + 1);
      if (n->alternate.has_value()) {
        DumpNode(*n->alternate, out, depth + 1);
      }
      break;
    }
    case ASTNode::Kind::kWhileStmt: {
      const auto* n = static_cast<const ASTWhileStmt*>(node);
      absl::StrAppendFormat(&out, "%sWhileStmt\n", indent);
      DumpNode(n->condition, out, depth + 1);
      DumpNode(n->block, out, depth + 1);
      break;
    }
    case ASTNode::Kind::kConstExpr: {
      const auto* n = static_cast<const ASTConstExpr*>(node);
      switch (n->value->value_kind) {
        case ValueKind::kSignedInt:
          absl::StrAppendFormat(&out, "%sConstExpr [%d]\n", indent,
                                n->value->int_v);
          break;
        case ValueKind::kUnsignedInt:
          absl::StrAppendFormat(&out, "%sConstExpr [%u]\n", indent,
                                n->value->uint_v);
          break;
        case ValueKind::kFloat:
          absl::StrAppendFormat(&out, "%sConstExpr [%g]\n", indent,
                                n->value->float_v);
          break;
        case ValueKind::kBool:
          absl::StrAppendFormat(&out, "%sConstExpr [%s]\n", indent,
                                n->value->bool_v ? "true" : "false");
          break;
        default:
          ABSL_UNREACHABLE();
      }
      break;
    }
    case ASTNode::Kind::kBinaryExpr: {
      const auto* n = static_cast<const ASTBinaryExpr*>(node);
      absl::StrAppendFormat(&out, "%sBinaryExpr [%s]\n", indent,
                            GetBinaryOpMnemonic(n->op));
      DumpNode(n->left, out, depth + 1);
      DumpNode(n->right, out, depth + 1);
      break;
    }
    case ASTNode::Kind::kUnaryExpr: {
      const auto* n = static_cast<const ASTUnaryExpr*>(node);
      absl::StrAppendFormat(&out, "%sUnaryExpr [%s]\n", indent,
                            GetUnaryOpMnemonic(n->op));
      DumpNode(n->expr, out, depth + 1);
      break;
    }
    case ASTNode::Kind::kGroupExpr: {
      const auto* n = static_cast<const ASTGroupExpr*>(node);
      absl::StrAppendFormat(&out, "%sGroupExpr\n", indent);
      DumpNode(n->expr, out, depth + 1);
      break;
    }
    case ASTNode::Kind::kIdentExpr: {
      const auto* n = static_cast<const ASTIdentExpr*>(node);
      absl::StrAppendFormat(&out, "%sIdentExpr [%s]\n", indent, n->ident);
      break;
    }
    case ASTNode::Kind::kAssignExpr: {
      const auto* n = static_cast<const ASTAssignExpr*>(node);
      absl::StrAppendFormat(&out, "%sAssignExpr [%s]\n", indent,
                            GetAssignOpMnemonic(n->op));
      DumpNode(n->target, out, depth + 1);
      DumpNode(n->value, out, depth + 1);
      break;
    }
    case ASTNode::Kind::kCallExpr: {
      const auto* n = static_cast<const ASTCallExpr*>(node);
      absl::StrAppendFormat(&out, "%sCallExpr\n", indent);
      DumpNode(n->callee, out, depth + 1);
      for (int i = 0; i < n->args.size(); i++) {
        DumpNode(n->args[i], out, depth + 1);
      }
      break;
    }
    case ASTNode::Kind::kTemplateExpr: {
      const auto* n = static_cast<const ASTTemplateExpr*>(node);
      absl::StrAppendFormat(&out, "%sTemplateExpr\n", indent);
      for (int i = 0; i < n->segments.size(); i++) {
        const auto& seg = n->segments[i];
        if (seg.kind == ASTTemplateExpr::Segment::kPart) {
          absl::StrAppendFormat(&out, "%s  Part [%s]\n", indent, seg.str_v);
        } else {
          absl::StrAppendFormat(&out, "%s  Expr\n", indent);
          DumpNode(seg.expr_v, out, depth + 2);
        }
      }
      break;
    }
    case ASTNode::Kind::kArrayExpr: {
      const auto* n = static_cast<const ASTArrayExpr*>(node);
      absl::StrAppendFormat(&out, "%sArrayExpr\n", indent);
      DumpNode(n->target, out, depth + 1);
      for (int i = 0; i < n->args.size(); i++) {
        DumpNode(n->args[i], out, depth + 1);
      }
      break;
    }
    case ASTNode::Kind::kNewArrayExpr: {
      const auto* n = static_cast<const ASTNewArrayExpr*>(node);
      absl::StrAppendFormat(&out, "%sNewArrayExpr\n", indent);
      for (int i = 0; i < n->elements_.size(); i++) {
        DumpNode(n->elements_[i], out, depth + 1);
      }
      break;
    }
    default:
      ABSL_UNREACHABLE();
  }
}

std::string DumpAST(const ASTNode* node) {
  std::string out = "\n";
  DumpNode(node, out, 0);
  return out;
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
