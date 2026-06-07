//
// Created by nothingbutyou on 5/17/26.
//

#include "codegen.h"

#include <iostream>

#include "absl/strings/str_format.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {
namespace {
constexpr auto kDebugDisassembly = true;
constexpr auto kDebugDisplayLines = false;
}  // namespace

GCPtr<FunctionObject> CodeGenerator::CompileFunctionObject(
    ZonePtr<ASTFunctionDecl> function_decl) {
  CHECK(function_decl->function_scope != nullptr)
      << "function_decl has not been processed by ScopeAnalyzer";
  const auto user_max_locals =
      std::max(function_decl->function_scope->max_locals(),
               function_decl->params.size());
  const auto code_object = CompileCodeObject(
      function_decl->name, function_decl->params.size(), user_max_locals, [&] {
        const auto _ = ScopeGuard(this, function_decl->function_scope);
        EmitContextEntry(function_decl->function_scope);
        CompileStmt(function_decl->block);
      });
  const auto interned_name =
      runtime_->GetPermanentZone()->InternString(function_decl->name);
  return runtime_->gc()->New<FunctionObject>(
      interned_name, code_object, current_module_, ancestor_aliases_);
}

GCPtr<FunctionObject> CodeGenerator::CompileInitObject(
    ZonePtrList<ASTGlobalDecl> globals,
    ZonePtrList<ASTModuleDecl> inner_modules) {
  constexpr auto func_name = "__module_init";
  constexpr auto guard_name = "__init_guard";
  const auto code_object = CompileCodeObject(func_name, 1, 1, [&] {
    // args:
    // 0 - uninitialized module object instance

    // static initialization guard
    const auto init_label = builder_.NewLabel();
    builder_.EmitVarLocal(Opcode::kLoadLocal, 0);
    builder_.EmitPushConst(ConstantDesc::CreateString(guard_name));
    builder_.Emit(Opcode::kGetField);

    builder_.EmitPushConst(ConstantDesc::CreateBool(true));
    builder_.Emit(Opcode::kCmpEq);
    builder_.EmitJump(Opcode::kJmpIfFalse, init_label);
    builder_.EmitPushConst(ConstantDesc::CreateNull());
    builder_.Emit(Opcode::kReturn);

    builder_.BindLabel(init_label);

    builder_.EmitVarLocal(Opcode::kLoadLocal, 0);
    builder_.EmitPushConst(ConstantDesc::CreateString(guard_name));
    builder_.EmitPushConst(ConstantDesc::CreateBool(true));
    builder_.Emit(Opcode::kSetField);
    // init globals
    for (const auto global : globals) {
      builder_.EmitVarLocal(Opcode::kLoadLocal, 0);
      builder_.EmitPushConst(ConstantDesc::CreateString(global->name));
      CompileExpr(global->init);
      builder_.Emit(Opcode::kSetField);
    }

    // init inner modules
    for (const auto inner_module : inner_modules) {
      // callee: inner_module_obj.__module_init
      builder_.EmitVarLocal(Opcode::kLoadLocal, 0);
      builder_.EmitPushConst(ConstantDesc::CreateString(inner_module->name));
      builder_.Emit(Opcode::kGetField);
      builder_.EmitPushConst(ConstantDesc::CreateString(func_name));
      builder_.Emit(Opcode::kGetField);

      // argument: inner_module_obj
      builder_.EmitVarLocal(Opcode::kLoadLocal, 0);
      builder_.EmitPushConst(ConstantDesc::CreateString(inner_module->name));
      builder_.Emit(Opcode::kGetField);

      builder_.EmitInvoke(Opcode::kInvoke, 1);
      builder_.Emit(Opcode::kPop);
    }
  });
  const auto interned_init_name =
      runtime_->GetPermanentZone()->InternString(func_name);
  return runtime_->gc()->New<FunctionObject>(
      interned_init_name, code_object, current_module_, ancestor_aliases_);
}

ZonePtr<CodeObject> CodeGenerator::CompileImportStub(ZoneStr name) {
  return CompileCodeObject(name, 1, 1, [&] {
    const auto module_slot = AllocateSyntheticSlot();

    // __load_module(name) -> module
    builder_.EmitPushConst(ConstantDesc::CreateString("__load_module"));
    builder_.Emit(Opcode::kLoadGlobal);
    builder_.EmitVarLocal(Opcode::kLoadLocal, 0);
    builder_.EmitInvoke(Opcode::kInvoke, 1);
    builder_.EmitVarLocal(Opcode::kStoreLocal, module_slot);

    // module.__module_init(module) — idempotent via __init_guard
    builder_.EmitVarLocal(Opcode::kLoadLocal, module_slot);
    builder_.EmitPushConst(ConstantDesc::CreateString("__module_init"));
    builder_.Emit(Opcode::kGetField);
    builder_.EmitVarLocal(Opcode::kLoadLocal, module_slot);
    builder_.EmitInvoke(Opcode::kInvoke, 1);
    builder_.Emit(Opcode::kPop);

    builder_.EmitVarLocal(Opcode::kLoadLocal, module_slot);
    builder_.Emit(Opcode::kReturn);
  });
}

ZonePtr<CodeObject> CodeGenerator::CompileCodeObject(ZoneStr debug_name,
                                                     int params,
                                                     int user_max_locals,
                                                     CodeGenFn op) {
  next_synthetic_slot_ = user_max_locals;
  max_locals_total_ = user_max_locals;

  op();

  // native return check
  const auto need_tail_return =
      (!builder_.instructions().empty() &&
       builder_.instructions()[builder_.instructions().size() - 1].op !=
           Opcode::kReturn) ||
      builder_.instructions().empty();
  if (need_tail_return) {
    builder_.EmitPushConst(ConstantDesc::CreateNull());
    builder_.Emit(Opcode::kReturn);
  }

  const auto rt_zone = runtime_->GetPermanentZone();
  const auto instructions = FreezeInstructions(rt_zone, builder_);
  const auto constants = FreezeConstants(rt_zone, builder_);
  const auto code_object = runtime_->CreateCodeObject(
      instructions, constants,
      rt_zone->CopyArray(builder_.debug_info().ToSpan()),
      rt_zone->CopyArray(try_catch_blocks_.ToSpan()),
      params,                                         // arg count
      ComputeMaxStackDepth(instructions, constants),  // max stack TODO
      std::max(max_locals_total_, params));

  if (kDebugDisassembly) {
    std::cout << absl::StrFormat("Disassembly `%s`\n", debug_name);
    BytecodeDisassembler(code_object, kDebugDisplayLines)
        .Disassemble(std::cout);
    std::cout << std::endl;
  }

  return code_object;
}

int CodeGenerator::AllocateSyntheticSlot() {
  const auto slot = next_synthetic_slot_++;
  if (slot + 1 > max_locals_total_) {
    max_locals_total_ = slot + 1;
  }
  return slot;
}
int CodeGenerator::ComputeContextDepth(Scope* declaring) {
  int depth = 0;
  for (Scope* scope = current_scope_; scope != declaring;
       scope = scope->parent()) {
    CHECK_NE(scope, nullptr) << "declaring scope is not an ancestor of current";
    if (scope->need_context()) {
      depth++;
    }
  }
  return depth;
}

int CodeGenerator::EmitContextEntry(Scope* scope) {
  if (!scope->need_context()) {
    return -1;
  }
  const auto save_slot = AllocateSyntheticSlot();
  builder_.Emit(Opcode::kMakeContext, 0, scope->num_context_slots());
  builder_.Emit(Opcode::kPushContext, 0, save_slot);
  if (scope->kind() == Scope::Kind::kFunction) {
    for (auto* variable : scope->variables()) {
      if (variable->kind == BindingKind::kParameter && variable->captured) {
        builder_.EmitVarLocal(Opcode::kLoadLocal, variable->slot);
        builder_.EmitVarContext(Opcode::kStoreContextSlot, 0,
                                variable->context_slot);
      }
    }
  }
  return save_slot;
}

void CodeGenerator::EmitContextExit(const int save_slot) {
  if (save_slot < 0) {
    return;
  }
  builder_.Emit(Opcode::kPopContext, 0, save_slot);
}

void CodeGenerator::CompileStmt(ZonePtr<ASTStmt> stmt) {
  MarkCurrentLine(stmt);
  switch (stmt->kind()) {
    case ASTNode::Kind::kBlockStmt: {
      const auto block_stmt = Cast<ASTBlockStmt>(stmt);
      const auto _ = ScopeGuard(this, block_stmt->scope);

      const auto save_slot = EmitContextEntry(block_stmt->scope);
      if (save_slot >= 0) {
        finally_blocks_.push_back(
            [this, save_slot] { EmitContextExit(save_slot); });
      }

      for (const auto inner_stmt : block_stmt->stmts) {
        CompileStmt(inner_stmt);
      }

      if (save_slot >= 0) {
        EmitContextExit(save_slot);
        finally_blocks_.pop_back();
      }

      break;
    }
    case ASTNode::Kind::kVarStmt: {
      const auto var_stmt = Cast<ASTVarStmt>(stmt);
      if (var_stmt->init_expr) {
        CompileExpr(*var_stmt->init_expr);
      } else {
        builder_.EmitPushConst(ConstantDesc::CreateNull());
      }
      const auto init_slot = AllocateSyntheticSlot();
      builder_.EmitVarLocal(Opcode::kStoreLocal, init_slot);
      CompilePattern(var_stmt->pattern, init_slot);
      break;
    }
    case ASTNode::Kind::kExprStmt: {
      const auto expr_stmt = Cast<ASTExprStmt>(stmt);
      CompileExpr(expr_stmt->expr);
      builder_.Emit(Opcode::kPop);
      break;
    }
    case ASTNode::Kind::kIfStmt: {
      const auto if_stmt = Cast<ASTIfStmt>(stmt);
      const auto false_label = builder_.NewLabel();
      const auto exit_label = builder_.NewLabel();
      const auto has_else_branch =
          if_stmt->alternate.has_value() || if_stmt->else_block.has_value();
      CompileExpr(if_stmt->condition);
      builder_.EmitJump(Opcode::kJmpIfFalse, false_label);
      CompileStmt(if_stmt->then);
      if (has_else_branch) {
        builder_.EmitJump(Opcode::kJmp, exit_label);
      }
      builder_.BindLabel(false_label);
      if (has_else_branch) {
        if (if_stmt->alternate) {
          CompileStmt(*if_stmt->alternate);
        } else if (if_stmt->else_block) {
          CompileStmt(*if_stmt->else_block);
        }
        builder_.BindLabel(exit_label);
      }
      break;
    }
    case ASTNode::Kind::kWhileStmt: {
      const auto while_stmt = Cast<ASTWhileStmt>(stmt);
      const auto label = builder_.NewLabel();
      const auto exit_label = builder_.NewLabel();
      builder_.BindLabel(label);
      CompileExpr(while_stmt->condition);
      builder_.EmitJump(Opcode::kJmpIfFalse, exit_label);
      CompileStmt(while_stmt->block);
      builder_.EmitJump(Opcode::kJmp, label);
      builder_.BindLabel(exit_label);
      break;
    }
    case ASTNode::Kind::kReturnStmt: {
      const auto return_stmt = Cast<ASTReturnStmt>(stmt);
      CompileExpr(return_stmt->expr);
      if (!finally_blocks_.empty()) {
        const auto ret_value = AllocateSyntheticSlot();
        builder_.EmitVarLocal(Opcode::kStoreLocal, ret_value);
        for (auto& finally_block : finally_blocks_) {
          finally_block();
        }
        builder_.EmitVarLocal(Opcode::kLoadLocal, ret_value);
      }
      builder_.Emit(Opcode::kReturn);
      break;
    }
    case ASTNode::Kind::kTryStmt: {
      const auto try_stmt = Cast<ASTTryStmt>(stmt);
      CompileTryStmt(try_stmt);
      break;
    }
    case ASTNode::Kind::kThrowStmt: {
      const auto throw_stmt = Cast<ASTThrowStmt>(stmt);
      CompileExpr(throw_stmt->expr);
      builder_.Emit(Opcode::kThrow);
      break;
    }
    default:
      ABSL_UNREACHABLE();
  }
}

void CodeGenerator::CompileExpr(ZonePtr<ASTExpr> expr) {
  MarkCurrentLine(expr);
  switch (expr->kind()) {
    case ASTNode::Kind::kConstExpr: {
      const auto const_expr = Cast<ASTConstExpr>(expr);
      const auto value = CompileConstant(const_expr->value);
      builder_.EmitPushConst(value);
      break;
    }
    case ASTNode::Kind::kBinaryExpr: {
      const auto binary_expr = Cast<ASTBinaryExpr>(expr);
      CompileBinaryExpr(binary_expr);
      break;
    }
    case ASTNode::Kind::kUnaryExpr: {
      const auto unary_expr = Cast<ASTUnaryExpr>(expr);
      CompileUnaryExpr(unary_expr);
      break;
    }
    case ASTNode::Kind::kGroupExpr: {
      const auto group_expr = Cast<ASTGroupExpr>(expr);
      CompileExpr(group_expr->expr);
      break;
    }
    case ASTNode::Kind::kIdentExpr: {
      const auto ident_expr = Cast<ASTIdentExpr>(expr);
      CompileIdent(ident_expr, /*is_write=*/false);
      break;
    }
    case ASTNode::Kind::kCallExpr: {
      const auto call_expr = Cast<ASTCallExpr>(expr);
      CompileExpr(call_expr->callee);
      for (const auto arg : call_expr->args) {
        CompileExpr(arg);
      }
      builder_.EmitInvoke(Opcode::kInvoke, call_expr->args.size());
      break;
    }
    case ASTNode::Kind::kAssignExpr: {
      const auto assign_expr = Cast<ASTAssignExpr>(expr);
      CompileAssignExpr(assign_expr);
      break;
    }
    case ASTNode::Kind::kTemplateExpr: {
      const auto template_expr = Cast<ASTTemplateExpr>(expr);
      CompileTemplateExpr(template_expr);
      break;
    }
    case ASTNode::Kind::kNewArrayExpr: {
      const auto new_expr = Cast<ASTNewArrayExpr>(expr);
      builder_.EmitPushConst(
          ConstantDesc::CreateUInt(new_expr->elements.size()));
      builder_.Emit(Opcode::kNewArray);
      for (auto n = 0; n < new_expr->elements.size(); n++) {
        builder_.Emit(Opcode::kDup);
        builder_.EmitPushConst(ConstantDesc::CreateUInt(n));
        CompileExpr(new_expr->elements[n]);
        builder_.Emit(Opcode::kStoreArray);
      }
      break;
    }
    case ASTNode::Kind::kArrayExpr: {
      CompileRValue(expr);
      break;
    }
    case ASTNode::Kind::kNewObjectExpr: {
      builder_.Emit(Opcode::kNewObject);
      break;
    }
    case ASTNode::Kind::kMemberAccessExpr: {
      CompileRValue(expr);
      break;
    }
    case ASTNode::Kind::kClosureExpr: {
      const auto closure_expr = Cast<ASTClosureExpr>(expr);
      CompileClosureExpr(closure_expr);
      break;
    }
    case ASTNode::Kind::kTupleExpr: {
      const auto tuple_expr = Cast<ASTTupleExpr>(expr);
      for (auto element : tuple_expr->elements) {
        CompileExpr(element);
      }
      builder_.Emit(Opcode::kNewTuple, tuple_expr->elements.size(), 0);
      break;
    }
    default:
      ABSL_UNREACHABLE();
  }
}

void CodeGenerator::CompileBinaryExpr(ZonePtr<ASTBinaryExpr> expr) {
  // special cases
  if (expr->op == ASTBinaryExpr::Operator::kExp) {
    builder_.EmitPushConst(ConstantDesc::CreateString("__builtin_exp"));
    builder_.Emit(Opcode::kLoadGlobal);
    CompileExpr(expr->left);
    CompileExpr(expr->right);
    builder_.EmitInvoke(Opcode::kInvoke, 2);
    return;
  }

  // TODO: could be separated opcode
  if (expr->op == ASTBinaryExpr::Operator::kCmpNe) {
    CompileExpr(expr->left);
    CompileExpr(expr->right);
    builder_.Emit(Opcode::kCmpEq);
    builder_.Emit(Opcode::kNot);
    return;
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

  CompileExpr(expr->left);
  CompileExpr(expr->right);
  builder_.Emit(opcode);
}

void CodeGenerator::CompileUnaryExpr(ZonePtr<ASTUnaryExpr> expr) {
  CompileExpr(expr->expr);
  switch (expr->op) {
    case ASTUnaryExpr::Operator::kPlus: {
      // noop, for now
      break;
    }
    case ASTUnaryExpr::Operator::kNeg: {
      builder_.Emit(Opcode::kNeg);
      break;
    }
    case ASTUnaryExpr::Operator::kLogicNeg: {
      builder_.Emit(Opcode::kNot);
      break;
    }
    case ASTUnaryExpr::Operator::kBitwiseNeg: {
      // ~x = x ^ (0xFFFF...)
      // JVM does exactly the same
      builder_.EmitPushConst(ConstantDesc::CreateUInt(~uint64_t{0}));
      builder_.Emit(Opcode::kXor);
      break;
    }
  }
}

void CodeGenerator::CompileAssignExpr(ZonePtr<ASTAssignExpr> expr) {
  if (expr->op == ASTAssignExpr::Operator::kAssign) {
    CompileExpr(expr->value);
    CompileLValue(expr->target);
    return;
  }

  Opcode op;
  switch (expr->op) {
    case ASTAssignExpr::Operator::kAddAssign:
      op = Opcode::kAdd;
      break;
    case ASTAssignExpr::Operator::kSubAssign:
      op = Opcode::kSub;
      break;
    case ASTAssignExpr::Operator::kMulAssign:
      op = Opcode::kMul;
      break;
    case ASTAssignExpr::Operator::kDivAssign:
      op = Opcode::kDiv;
      break;
    case ASTAssignExpr::Operator::kModAssign:
      op = Opcode::kMod;
      break;
    case ASTAssignExpr::Operator::kAndAssign:
      op = Opcode::kAnd;
      break;
    case ASTAssignExpr::Operator::kOrAssign:
      op = Opcode::kOr;
      break;
    case ASTAssignExpr::Operator::kXorAssign:
      op = Opcode::kXor;
      break;
    case ASTAssignExpr::Operator::kShlAssign:
      op = Opcode::kShl;
      break;
    case ASTAssignExpr::Operator::kShrAssign:
      op = Opcode::kShr;
      break;
    default:
      ABSL_UNREACHABLE();
  }

  CompileRValue(expr->target);
  CompileExpr(expr->value);
  builder_.Emit(op);
  CompileLValue(expr->target);
}

void CodeGenerator::CompileTemplateExpr(ZonePtr<ASTTemplateExpr> expr) {
  // is that even possible?
  if (expr->segments.empty()) {
    builder_.EmitPushConst(ConstantDesc::CreateString(""));
    return;
  }

  if (expr->segments.size() == 1 &&
      expr->segments[0].kind == ASTTemplateExpr::Segment::kPart) {
    builder_.EmitPushConst(ConstantDesc::CreateString(expr->segments[0].str_v));
    return;
  }

  const auto segments = expr->segments;
  auto push_segments = [&](const ASTTemplateExpr::Segment& segment) {
    switch (segment.kind) {
      case ASTTemplateExpr::Segment::kPart: {
        builder_.EmitPushConst(ConstantDesc::CreateString(segment.str_v));
        break;
      }
      case ASTTemplateExpr::Segment::kExpr: {
        CompileExpr(segment.expr_v);
        break;
      }
    }
  };

  // TODO: make vm intrinsic from this
  const auto acc = AllocateSyntheticSlot();
  builder_.EmitPushConst(ConstantDesc::CreateString(""));
  builder_.EmitVarLocal(Opcode::kStoreLocal, acc);
  for (const auto& segment : segments) {
    builder_.EmitVarLocal(Opcode::kLoadLocal, acc);
    push_segments(segment);
    builder_.Emit(Opcode::kAdd);
    builder_.EmitVarLocal(Opcode::kStoreLocal, acc);
  }
  builder_.EmitVarLocal(Opcode::kLoadLocal, acc);
}

void CodeGenerator::CompileLValue(ZonePtr<ASTExpr> target) {
  if (target->kind() == ASTNode::Kind::kIdentExpr) {
    const auto ident_expr = Cast<ASTIdentExpr>(target);
    builder_.Emit(Opcode::kDup);
    CompileIdent(ident_expr, /*is_write=*/true);
  } else if (target->kind() == ASTNode::Kind::kArrayExpr) {
    const auto array_expr = Cast<ASTArrayExpr>(target);
    if (array_expr->args.size() != 1) {
      reporter_->Report(
          Diagnostic::Error(
              source_file_,
              "Only single dimensional array access operator is supported")
              .WithLabel(target->span(),
                         "invalid operand count for `[...]` operator"));
      return;
    }
    // TODO: add more stack operation opcodes?
    builder_.Emit(Opcode::kDup);
    CompileExpr(array_expr->target);
    builder_.Emit(Opcode::kSwap);
    CompileExpr(array_expr->args[0]);
    builder_.Emit(Opcode::kSwap);
    builder_.Emit(Opcode::kStoreArray);
  } else if (target->kind() == ASTNode::Kind::kMemberAccessExpr) {
    const auto member_exor = Cast<ASTMemberAccessExpr>(target);
    builder_.Emit(Opcode::kDup);
    CompileExpr(member_exor->expr);
    builder_.Emit(Opcode::kSwap);
    builder_.EmitPushConst(ConstantDesc::CreateString(member_exor->field));
    builder_.Emit(Opcode::kSwap);
    builder_.Emit(Opcode::kSetField);
  } else {
    reporter_->Report(
        Diagnostic::Error(source_file_,
                          "LValue can be only "
                          "local variables or global symbols")
            .WithLabel(target->span(), "non-assignable left expression"));
  }
}

void CodeGenerator::CompileRValue(ZonePtr<ASTExpr> value) {
  if (value->kind() == ASTNode::Kind::kIdentExpr) {
    const auto ident_expr = Cast<ASTIdentExpr>(value);
    CompileIdent(ident_expr, /*is_write=*/false);
  } else if (value->kind() == ASTNode::Kind::kArrayExpr) {
    const auto array_expr = Cast<ASTArrayExpr>(value);
    if (array_expr->args.size() != 1) {
      reporter_->Report(
          Diagnostic::Error(
              source_file_,
              "Only single dimensional array access operator is supported")
              .WithLabel(value->span(),
                         "invalid operand count for `[...]` operator"));
      return;
    }
    CompileExpr(array_expr->target);
    CompileExpr(array_expr->args[0]);
    builder_.Emit(Opcode::kLoadArray);
  } else if (value->kind() == ASTNode::Kind::kMemberAccessExpr) {
    const auto member_exor = Cast<ASTMemberAccessExpr>(value);
    CompileExpr(member_exor->expr);
    builder_.EmitPushConst(ConstantDesc::CreateString(member_exor->field));
    builder_.Emit(Opcode::kGetField);
  } else {
    reporter_->Report(
        Diagnostic::Error(source_file_,
                          "RValue can be only "
                          "local variables or global symbols")
            .WithLabel(value->span(), "non-assignable left expression"));
  }
}
void CodeGenerator::CompileClosureExpr(ZonePtr<ASTClosureExpr> expr) {
  CHECK(expr->function_scope != nullptr)
      << "closure_expr has not been processed by ScopeAnalyzer";
  CodeGenerator generator(runtime_, reporter_, zone_, source_file_,
                          current_module_, ancestor_aliases_);
  const auto closure_name = "closureXXX";  // TODO: proper closure name
  const auto user_max_locals = std::max(expr->function_scope->max_locals(),
                                        static_cast<int>(expr->params.size()));
  const auto code_object = generator.CompileCodeObject(
      closure_name, expr->params.size(), user_max_locals, [&] {
        const auto _ = ScopeGuard(&generator, expr->function_scope);
        generator.EmitContextEntry(expr->function_scope);
        generator.CompileStmt(expr->block);
      });
  const auto function_object = runtime_->gc()->New<FunctionObject>(
      closure_name, code_object, current_module_, ancestor_aliases_);
  // FIXME: for now mark as permanent, lifetime is hard here...
  runtime_->gc()->MarkPermanent(function_object);
  const auto constant_idx =
      builder_.AddConstant(ConstantDesc::CreateFunction(function_object));
  builder_.Emit(Opcode::kClosure, 0, constant_idx);
}

void CodeGenerator::CompileIdent(ZonePtr<ASTIdentExpr> ident,
                                 const bool is_write) {
  CHECK(ident->binding != nullptr)
      << "ident has not been processed by ScopeAnalyzer";
  const auto binding = ident->binding;
  switch (binding->kind) {
    case BindingKind::kParameter:
    case BindingKind::kLocal: {
      if (binding->captured) {
        CHECK(binding->owning_scope != nullptr);
        const auto depth = ComputeContextDepth(binding->owning_scope);
        builder_.EmitVarContext(
            is_write ? Opcode::kStoreContextSlot : Opcode::kLoadContextSlot,
            depth, binding->context_slot);
      } else {
        builder_.EmitVarLocal(
            is_write ? Opcode::kStoreLocal : Opcode::kLoadLocal, binding->slot);
      }
      return;
    }
    case BindingKind::kModuleGlobal:
    case BindingKind::kGlobal: {
      builder_.EmitPushConst(ConstantDesc::CreateString(binding->name));
      builder_.Emit(is_write ? Opcode::kStoreGlobal : Opcode::kLoadGlobal);
      return;
    }
  }
}

void CodeGenerator::CompileTryStmt(ZonePtr<ASTTryStmt> stmt) {
  const auto emit_finally = [&] {
    for (auto finally_block : stmt->finally_blocks) {
      CompileStmt(finally_block);
    }
  };

  if (!stmt->finally_blocks.empty()) {
    finally_blocks_.push_back(emit_finally);
  }

  struct CatchRange {
    int begin_bci;
    int end_bci;
  };

  const auto after = builder_.NewLabel();

  // try
  const auto try_begin_bci = builder_.current_bci();
  CompileStmt(stmt->try_block);
  const auto try_end_bci = builder_.current_bci();
  emit_finally();
  builder_.EmitJump(Opcode::kJmp, after);

  // catch blocks
  int first_catch_begin_bci = -1;
  ZoneList<CatchRange> catch_ranges(zone_);
  for (const auto& catch_block : stmt->catch_blocks) {
    const auto _ = ScopeGuard(this, catch_block.scope);
    const auto catch_begin_bci = builder_.current_bci();
    if (first_catch_begin_bci < 0) {
      first_catch_begin_bci = catch_begin_bci;
    }

    builder_.Emit(Opcode::kPushException);
    if (catch_block.var_name != nullptr &&
        catch_block.var_name->kind() == ASTNode::Kind::kIdentExpr) {
      const auto ident = Cast<ASTIdentExpr>(catch_block.var_name);
      CHECK(ident->binding != nullptr)
          << "catch var has not been processed by ScopeAnalyzer";
      builder_.EmitVarLocal(Opcode::kStoreLocal, ident->binding->slot);
    } else {
      builder_.Emit(Opcode::kPop);
    }
    builder_.Emit(Opcode::kClearException);

    CompileStmt(catch_block.block);
    const auto catch_end_bci = builder_.current_bci();
    catch_ranges.Add(zone_, CatchRange{catch_begin_bci, catch_end_bci});
    emit_finally();
    builder_.EmitJump(Opcode::kJmp, after);
  }

  // handlers
  if (first_catch_begin_bci >= 0) {
    try_catch_blocks_.Add(zone_, TryCatchBlock{try_begin_bci, try_end_bci,
                                               first_catch_begin_bci});
  } else {
    const auto handler_bci = builder_.current_bci();
    emit_finally();
    builder_.Emit(Opcode::kRethrow);
    try_catch_blocks_.Add(
        zone_, TryCatchBlock{try_begin_bci, try_end_bci, handler_bci});
  }

  for (auto catch_range : catch_ranges) {
    const auto handler_bci = builder_.current_bci();
    emit_finally();
    builder_.Emit(Opcode::kRethrow);
    try_catch_blocks_.Add(
        zone_,
        TryCatchBlock{catch_range.begin_bci, catch_range.end_bci, handler_bci});
  }

  builder_.BindLabel(after);

  if (!stmt->finally_blocks.empty()) {
    finally_blocks_.pop_back();
  }
}
void CodeGenerator::CompilePattern(ZonePtr<ASTPattern> pattern, int right_lv) {
  if (Is<ASTBindingPattern>(pattern)) {
    const auto binding_pattern = Cast<ASTBindingPattern>(pattern);
    builder_.EmitVarLocal(Opcode::kLoadLocal, right_lv);
    const auto binding = binding_pattern->binding;
    if (binding->captured) {
      builder_.EmitVarContext(Opcode::kStoreContextSlot, 0,
                              binding->context_slot);
    } else {
      builder_.EmitVarLocal(Opcode::kStoreLocal, binding->slot);
    }
  } else if (Is<ASTTuplePattern>(pattern)) {
    CHECK_GE(right_lv, 0);
    const auto tuple_pattern = Cast<ASTTuplePattern>(pattern);
    for (auto n = 0; n < tuple_pattern->patterns.size(); ++n) {
      builder_.EmitVarLocal(Opcode::kLoadLocal, right_lv);
      builder_.EmitPushConst(ConstantDesc::CreateUInt(n));
      builder_.Emit(Opcode::kLoadArray);

      const auto tmp = AllocateSyntheticSlot();
      builder_.EmitVarLocal(Opcode::kStoreLocal, tmp);
      CompilePattern(tuple_pattern->patterns[n], tmp);
      // TODO: release synthetic slot here
    }
  }
}

ConstantDesc CodeGenerator::CompileConstant(ZonePtr<Token> token) {
  // TODO: signed/unsigned
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
      return {.kind = ConstantDesc::Kind::kBool, .bool_v = token->bool_v};
    case ValueKind::kStrSegment:
      return {.kind = ConstantDesc::Kind::kString, .str_v = token->str_v};
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

void CodeGenerator::MarkCurrentLine(const ZonePtr<ASTNode> node) {
  builder_.current_line(source_file_->LocationOf(node->span().offset).line);
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
