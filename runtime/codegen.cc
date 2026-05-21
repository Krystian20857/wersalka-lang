//
// Created by nothingbutyou on 5/17/26.
//

#include "codegen.h"

#include <iostream>

#include "runtime/sym.h"
#include "runtime/zone.h"

#include "absl/strings/str_format.h"

namespace wersalka {
namespace lang {
namespace runtime {
namespace {
constexpr auto kDebugDisassembly = true;
}

ZonePtr<CodeObject> CodeGenerator::CreateCodeObject(
    ZonePtr<ASTFunctionDecl> ast_func) {
  Zone builder_zone;
  BytecodeBuilder builder(&builder_zone);
  LocalsTable locals(&builder_zone, 0);

  // push args, args starts from local index 0
  for (const auto arg : ast_func->params) {
    locals.DefineVar(arg);
  }

  CompileStmt(builder_zone, builder, locals, ast_func->block);

  // add nop so jumps don't go over insn count
  builder.Emit(Opcode::kNop);

  if (kDebugDisassembly) {
    std::cout << absl::StrFormat("Disassembly `%s`\n", ast_func->name);
    BytecodeDisassembler(builder.instructions(), builder.constants())
        .Disassemble(std::cout);
  }

  const auto rt_zone = runtime_->GetPermanentZone();
  return rt_zone->New<CodeObject>(FreezeInstructions(rt_zone, builder),
                                  FreezeConstants(rt_zone, builder),
                                  ast_func->params.size(),  // arg count
                                  0,                        // max stack TODO
                                  locals.max_locals());
}
void CodeGenerator::CompileStmt(Zone& zone, BytecodeBuilder& builder,
                                LocalsTable& locals, ZonePtr<ASTStmt> stmt) {
  switch (stmt->kind()) {
    case ASTNode::Kind::kBlockStmt: {
      const auto block_stmt = Cast<ASTBlockStmt>(stmt);
      locals.PushScope();
      for (const auto inner_stmt : block_stmt->stmts) {
        CompileStmt(zone, builder, locals, inner_stmt);
      }
      locals.PopScope();
      break;
    }
    case ASTNode::Kind::kVarStmt: {
      const auto var_stmt = Cast<ASTVarStmt>(stmt);
      const auto local_idx = locals.DefineVar(var_stmt->name);
      if (var_stmt->init_expr) {
        CompileExpr(zone, builder, locals, *var_stmt->init_expr);
      } else {
        builder.EmitPushConst(ConstantDesc::CreateNull());
      }
      builder.EmitVarLocal(Opcode::kStoreLocal, local_idx);
      break;
    }
    case ASTNode::Kind::kExprStmt: {
      const auto expr_stmt = Cast<ASTExprStmt>(stmt);
      CompileExpr(zone, builder, locals, expr_stmt->expr);
      builder.Emit(Opcode::kPop);
      break;
    }
    case ASTNode::Kind::kIfStmt: {
      const auto if_stmt = Cast<ASTIfStmt>(stmt);
      const auto false_label = builder.NewLabel();
      const auto exit_label = builder.NewLabel();
      const auto has_else_branch =
          if_stmt->alternate.has_value() || if_stmt->else_block.has_value();
      CompileExpr(zone, builder, locals, if_stmt->condition);
      builder.EmitJump(Opcode::kJmpIfFalse, false_label);
      CompileStmt(zone, builder, locals, if_stmt->then);
      if (has_else_branch) {
        builder.EmitJump(Opcode::kJmp, exit_label);
      }
      builder.BindLabel(false_label);
      if (has_else_branch) {
        if (if_stmt->alternate) {
          CompileStmt(zone, builder, locals, *if_stmt->alternate);
        } else if (if_stmt->else_block) {
          CompileStmt(zone, builder, locals, *if_stmt->else_block);
        }
        builder.BindLabel(exit_label);
      }
      break;
    }
    case ASTNode::Kind::kWhileStmt: {
      const auto while_stmt = Cast<ASTWhileStmt>(stmt);
      const auto label = builder.NewLabel();
      const auto exit_label = builder.NewLabel();
      builder.BindLabel(label);
      CompileExpr(zone, builder, locals, while_stmt->condition);
      builder.EmitJump(Opcode::kJmpIfFalse, exit_label);
      CompileStmt(zone, builder, locals, while_stmt->block);
      builder.EmitJump(Opcode::kJmp, label);
      builder.BindLabel(exit_label);
      break;
    }
    case ASTNode::Kind::kReturnStmt: {
      const auto return_stmt = Cast<ASTReturnStmt>(stmt);
      CompileExpr(zone, builder, locals, return_stmt->expr);
      builder.Emit(Opcode::kReturn);
      break;
    }
    default:
      ABSL_UNREACHABLE();
  }
}
void CodeGenerator::CompileExpr(Zone& zone, BytecodeBuilder& builder,
                                LocalsTable& locals, ZonePtr<ASTExpr> expr) {
  switch (expr->kind()) {
    case ASTNode::Kind::kConstExpr: {
      const auto const_expr = Cast<ASTConstExpr>(expr);
      const auto value = CompileConstant(builder, const_expr->value);
      builder.EmitPushConst(value);
      break;
    }
    case ASTNode::Kind::kBinaryExpr: {
      const auto binary_expr = Cast<ASTBinaryExpr>(expr);
      CompileBinaryExpr(zone, builder, locals, binary_expr);
      break;
    }
    case ASTNode::Kind::kUnaryExpr: {
      const auto unary_expr = Cast<ASTUnaryExpr>(expr);
      CompileUnaryExpr(zone, builder, locals, unary_expr);
      break;
    }
    case ASTNode::Kind::kGroupExpr: {
      const auto group_expr = Cast<ASTGroupExpr>(expr);
      CompileExpr(zone, builder, locals, group_expr->expr);
      break;
    }
    case ASTNode::Kind::kIdentExpr: {
      const auto ident_expr = Cast<ASTIdentExpr>(expr);
      const auto slot = locals.LookupVar(ident_expr->ident);
      if (slot) {
        builder.EmitVarLocal(Opcode::kLoadLocal, *slot);
      } else {
        builder.EmitVarGlobal(Opcode::kLoadGlobal, ident_expr->ident);
      }
      break;
    }
    case ASTNode::Kind::kCallExpr: {
      const auto call_expr = Cast<ASTCallExpr>(expr);
      CompileExpr(zone, builder, locals, call_expr->callee);
      for (const auto arg : call_expr->args) {
        CompileExpr(zone, builder, locals, arg);
      }
      builder.Emit(Opcode::kInvoke);
      break;
    }
    case ASTNode::Kind::kAssignExpr: {
      const auto assign_expr = Cast<ASTAssignExpr>(expr);
      const auto target_expr = assign_expr->target;
      if (target_expr->kind() == ASTNode::Kind::kIdentExpr) {
        const auto ident_expr = Cast<ASTIdentExpr>(target_expr);
        const auto slot = locals.LookupVar(ident_expr->ident);
        if (slot) {
          builder.EmitVarLocal(Opcode::kStoreLocal, *slot);
        } else {
          builder.EmitVarGlobal(Opcode::kStoreGlobal, ident_expr->ident);
        }
      } else {
        // TODO: redirect to intrinsic when possible,
        //  I don't want to invent lvalue/rvalue semantics here
        reporter_->Report(Diagnostic::Error("Assignment is only possible on "
                                            "local variables or global symbols")
                              .withLabel(target_expr->span(),
                                         "non-assignable left expression"));
      }
      break;
    }
    default:
      ABSL_UNREACHABLE();
  }
}
void CodeGenerator::CompileBinaryExpr(Zone& zone, BytecodeBuilder& builder,
                                      LocalsTable& locals,
                                      ZonePtr<ASTBinaryExpr> expr) {
  // special cases
  if (expr->op == ASTBinaryExpr::Operator::kExp) {
    builder.EmitVarGlobal(Opcode::kLoadGlobal, "__builtin_exp");
    CompileExpr(zone, builder, locals, expr->left);
    CompileExpr(zone, builder, locals, expr->right);
    builder.Emit(Opcode::kInvoke);
    return;
  }

  // TODO: could be separated opcode
  if (expr->op == ASTBinaryExpr::Operator::kCmpNe) {
    CompileExpr(zone, builder, locals, expr->left);
    CompileExpr(zone, builder, locals, expr->right);
    builder.Emit(Opcode::kCmpEq);
    builder.Emit(Opcode::kNeg);
  }

  // normal cases
  Opcode opcode;
  switch (expr->op) {
    case ASTBinaryExpr::Operator::kAdd:
      opcode = Opcode::kAdd;
      break;
    case ASTBinaryExpr::Operator::kSub:
      opcode = Opcode::kSub;
      break;
    case ASTBinaryExpr::Operator::kMul:
      opcode = Opcode::kMul;
      break;
    case ASTBinaryExpr::Operator::kDiv:
      opcode = Opcode::kDiv;
      break;
    case ASTBinaryExpr::Operator::kMod:
      opcode = Opcode::kMod;
      break;
    case ASTBinaryExpr::Operator::kLogicAnd:
    case ASTBinaryExpr::Operator::kBitwiseAnd:
      opcode = Opcode::kAnd;
      break;
    case ASTBinaryExpr::Operator::kLogicOr:
    case ASTBinaryExpr::Operator::kBitwiseOr:
      opcode = Opcode::kOr;
      break;
    case ASTBinaryExpr::Operator::kBitwiseXor:
      opcode = Opcode::kXor;
      break;
    case ASTBinaryExpr::Operator::kBitwiseShl:
      opcode = Opcode::kShl;
      break;
    case ASTBinaryExpr::Operator::kBitwiseShr:
      opcode = Opcode::kShr;
      break;
    case ASTBinaryExpr::Operator::kCmpGt:
      opcode = Opcode::kCmpGt;
      break;
    case ASTBinaryExpr::Operator::kCmpLt:
      opcode = Opcode::kCmpLt;
      break;
    case ASTBinaryExpr::Operator::kCmpGe:
      opcode = Opcode::kCmpGe;
      break;
    case ASTBinaryExpr::Operator::kCmpLe:
      opcode = Opcode::kCmpLe;
      break;
    case ASTBinaryExpr::Operator::kCmpEq:
      opcode = Opcode::kCmpEq;
      break;
    default:
      ABSL_UNREACHABLE();
  }

  CompileExpr(zone, builder, locals, expr->left);
  CompileExpr(zone, builder, locals, expr->right);
  builder.Emit(opcode);
}
void CodeGenerator::CompileUnaryExpr(Zone& zone, BytecodeBuilder& builder,
                                     LocalsTable& locals,
                                     ZonePtr<ASTUnaryExpr> expr) {
  CompileExpr(zone, builder, locals, expr->expr);
  switch (expr->op) {
    case ASTUnaryExpr::Operator::kPlus: {
      // noop, for now
      break;
    }
    case ASTUnaryExpr::Operator::kNeg: {
      builder.Emit(Opcode::kNeg);
      break;
    }
    case ASTUnaryExpr::Operator::kLogicNeg: {
      builder.Emit(Opcode::kNeg);
      break;
    }
    case ASTUnaryExpr::Operator::kBitwiseNeg: {
      // ~x = x ^ (0xFFFF...)
      // JVM does exactly the same
      builder.EmitPushConst(ConstantDesc::CreateUInt(~uint64_t{0}));
      builder.Emit(Opcode::kXor);
      break;
    }
  }
}
ConstantDesc CodeGenerator::CompileConstant(BytecodeBuilder& builder,
                                            ZonePtr<Token> token) {
  switch (token->value_kind) {
    case ValueKind::kNull:
      return {.kind = ConstantDesc::Kind::kNull};
    case ValueKind::kUnsignedInt:
      return {.kind = ConstantDesc::Kind::kInt, .uint_v = token->uint_v};
    case ValueKind::kSignedInt:
      return {.kind = ConstantDesc::Kind::kInt, .int_v = token->int_v};
    case ValueKind::kFloat:
      return {.kind = ConstantDesc::Kind::kFloat, .float_v = token->float_v};
    case ValueKind::kBool:
      return {.kind = ConstantDesc::Kind::kFloat, .float_v = token->float_v};
    case ValueKind::kStrSegment:
      return {.kind = ConstantDesc::Kind::kFloat, .str_v = token->str_v};
    default:
      ABSL_UNREACHABLE();
  }
}
std::span<const ConstantDesc> CodeGenerator::FreezeConstants(
    Zone* zone, const BytecodeBuilder& builder) {
  ZoneList<ConstantDesc> list(zone, builder.constants().size());
  for (const auto constant : builder.constants()) {
    if (constant.kind == ConstantDesc::Kind::kString) {
      list.Add(zone,
               ConstantDesc::CreateString(zone->InternString(constant.str_v)));
    } else {
      list.Add(zone, constant);
    }
  }
  return list.ToSpan();
}
std::span<const Instr> CodeGenerator::FreezeInstructions(
    Zone* zone, const BytecodeBuilder& builder) {
  ZoneList<Instr> list(zone, builder.instructions().size());
  for (auto instr : builder.instructions()) {
    list.Add(zone, instr);
  }
  return list.ToSpan();
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
