//
// Created by nothingbutyou on 6/6/26.
//

#include "runtime/scope.h"

#include "absl/strings/str_format.h"
#include "runtime/ast.h"

namespace wersalka {
namespace lang {
namespace runtime {

Scope::Scope(Zone* zone, const Kind kind, Scope* parent)
    : zone_(zone),
      kind_(kind),
      parent_(parent),
      variables_(zone),
      members_(zone),
      free_slots_(zone),
      need_context_(false) {}

Variable* Scope::DeclareVariable(const std::string_view name,
                                 const BindingKind binding_kind,
                                 const TextSpan declaration_span) {
  const auto variable = zone_->New<Variable>();
  variable->name = name;
  variable->kind = binding_kind;
  variable->declaration_span = declaration_span;
  variable->owning_scope = this;
  if (binding_kind == BindingKind::kParameter ||
      binding_kind == BindingKind::kLocal) {
    variable->slot = AllocateSlot();
  }
  variables_.Add(zone_, variable);
  return variable;
}

Variable* Scope::LookupLocal(const std::string_view name) const {
  for (auto* variable : variables_) {
    if (variable->name == name) {
      return variable;
    }
  }
  return nullptr;
}

Variable* Scope::Lookup(const std::string_view name) const {
  for (const Scope* scope = this; scope != nullptr; scope = scope->parent_) {
    if (auto* variable = scope->LookupLocal(name)) {
      return variable;
    }
  }
  return nullptr;
}

int Scope::AllocateSlot() {
  if (kind_ != Kind::kFunction) {
    CHECK_NE(parent_, nullptr);
    return parent_->AllocateSlot();
  }
  int slot;
  if (!free_slots_.empty()) {
    slot = free_slots_.Back();
    free_slots_.PopBack();
  } else {
    slot = next_slot_++;
  }
  if (slot + 1 > max_user_slots_) {
    max_user_slots_ = slot + 1;
  }
  return slot;
}

void Scope::ReleaseSlot(const int slot) {
  if (kind_ != Kind::kFunction) {
    CHECK_NE(parent_, nullptr);
    parent_->ReleaseSlot(slot);
    return;
  }
  free_slots_.Add(zone_, slot);
}

int Scope::AllocateContextSlot() { return num_context_slots_++; }

void Scope::ReleaseOwnedSlots() {
  for (int idx = variables_.size() - 1; idx >= 0; idx--) {
    auto* variable = variables_[idx];
    if (variable->kind == BindingKind::kLocal ||
        variable->kind == BindingKind::kParameter) {
      ReleaseSlot(variable->slot);
    }
  }
}

Scope* Scope::EnclosingFunction() {
  for (Scope* scope = this; scope != nullptr; scope = scope->parent_) {
    if (scope->kind_ == Kind::kFunction) {
      return scope;
    }
  }
  return nullptr;
}

Scope* Scope::EnclosingModule() {
  for (Scope* scope = this; scope != nullptr; scope = scope->parent_) {
    if (scope->kind_ == Kind::kModule || scope->kind_ == Kind::kCompileUnit) {
      return scope;
    }
  }
  return nullptr;
}

ScopeAnalyzer::ScopeAnalyzer(Zone* zone, DiagnosticReporter* reporter,
                             const SourceFile* source_file)
    : zone_(zone), reporter_(reporter), source_file_(source_file) {}

Scope* ScopeAnalyzer::AnalyzeCompileUnit(
    ZonePtr<ASTCompileUnit> unit,
    const std::vector<Scope*>& enclosing_module_chain) {
  Scope* parent_scope =
      enclosing_module_chain.empty() ? nullptr : enclosing_module_chain.back();
  const auto compile_unit_scope =
      zone_->New<Scope>(zone_, Scope::Kind::kCompileUnit, parent_scope);
  unit->scope = compile_unit_scope;

  RegisterMembers(compile_unit_scope, unit->globals, unit->functions,
                  unit->inner_modules);

  for (const auto global_decl : unit->globals) {
    AnalyzeGlobalInit(global_decl, compile_unit_scope);
  }
  for (const auto function_decl : unit->functions) {
    AnalyzeFunction(function_decl, compile_unit_scope);
  }

  std::vector<Scope*> extended_chain = enclosing_module_chain;
  extended_chain.push_back(compile_unit_scope);
  for (const auto inner_module : unit->inner_modules) {
    AnalyzeModule(inner_module, extended_chain);
  }
  return compile_unit_scope;
}

Scope* ScopeAnalyzer::AnalyzeModule(
    ZonePtr<ASTModuleDecl> module_decl,
    const std::vector<Scope*>& enclosing_module_chain) {
  Scope* parent_scope =
      enclosing_module_chain.empty() ? nullptr : enclosing_module_chain.back();
  const auto module_scope =
      zone_->New<Scope>(zone_, Scope::Kind::kModule, parent_scope);
  module_scope->set_module_name(module_decl->name);
  module_decl->scope = module_scope;

  RegisterMembers(module_scope, module_decl->globals, module_decl->functions,
                  module_decl->inner_modules);

  for (const auto global_decl : module_decl->globals) {
    AnalyzeGlobalInit(global_decl, module_scope);
  }
  for (const auto function_decl : module_decl->functions) {
    AnalyzeFunction(function_decl, module_scope);
  }

  std::vector<Scope*> extended_chain = enclosing_module_chain;
  extended_chain.push_back(module_scope);
  for (const auto inner_module : module_decl->inner_modules) {
    AnalyzeModule(inner_module, extended_chain);
  }
  return module_scope;
}

void ScopeAnalyzer::RegisterMembers(
    Scope* module_scope, const ZonePtrList<ASTGlobalDecl>& globals,
    const ZonePtrList<ASTFunctionDecl>& functions,
    const ZonePtrList<ASTModuleDecl>& inner_modules) {
  for (const auto function_decl : functions) {
    module_scope->AddMember(function_decl->name);
  }
  for (const auto global_decl : globals) {
    module_scope->AddMember(global_decl->name);
  }
  for (const auto inner_module : inner_modules) {
    module_scope->AddMember(inner_module->name);
  }
}

void ScopeAnalyzer::AnalyzeFunction(ZonePtr<ASTFunctionDecl> function_decl,
                                    Scope* module_scope) {
  const auto function_scope =
      zone_->New<Scope>(zone_, Scope::Kind::kFunction, module_scope);
  function_decl->function_scope = function_scope;

  for (const auto param_name : function_decl->params) {
    function_scope->DeclareVariable(param_name, BindingKind::kParameter,
                                    function_decl->span());
  }

  AnalyzeStmt(function_decl->block, function_scope);
}
void ScopeAnalyzer::AnalyzeClosure(ZonePtr<ASTClosureExpr> closure_expr,
                                   Scope* outer_scope) {
  const auto closure_scope =
      zone_->New<Scope>(zone_, Scope::Kind::kFunction, outer_scope);
  closure_expr->function_scope = closure_scope;
  for (const auto param_name : closure_expr->params) {
    closure_scope->DeclareVariable(param_name, BindingKind::kParameter,
                                   closure_expr->span());
  }
  AnalyzeStmt(closure_expr->block, closure_scope);
}

void ScopeAnalyzer::AnalyzeGlobalInit(ZonePtr<ASTGlobalDecl> global_decl,
                                      Scope* module_scope) {
  // Module globals are initialized inside the synthetic `__module_init`
  // function (one frame per module). For name resolution inside the init
  // expression, the surrounding scope is the module itself.
  AnalyzeExpr(global_decl->init, module_scope);
}

void ScopeAnalyzer::AnalyzeStmt(ZonePtr<ASTStmt> stmt, Scope* current_scope) {
  switch (stmt->kind()) {
    case ASTNode::Kind::kBlockStmt: {
      const auto block_stmt = Cast<ASTBlockStmt>(stmt);
      const auto block_scope =
          zone_->New<Scope>(zone_, Scope::Kind::kBlock, current_scope);
      block_stmt->scope = block_scope;
      for (const auto inner_stmt : block_stmt->stmts) {
        AnalyzeStmt(inner_stmt, block_scope);
      }
      block_scope->ReleaseOwnedSlots();
      break;
    }
    case ASTNode::Kind::kVarStmt: {
      const auto var_stmt = Cast<ASTVarStmt>(stmt);
      if (var_stmt->init_expr) {
        AnalyzeExpr(*var_stmt->init_expr, current_scope);
      }
      AnalyzePattern(var_stmt->pattern, current_scope);
      break;
    }
    case ASTNode::Kind::kExprStmt: {
      const auto expr_stmt = Cast<ASTExprStmt>(stmt);
      AnalyzeExpr(expr_stmt->expr, current_scope);
      break;
    }
    case ASTNode::Kind::kIfStmt: {
      const auto if_stmt = Cast<ASTIfStmt>(stmt);
      AnalyzeExpr(if_stmt->condition, current_scope);
      AnalyzeStmt(if_stmt->then, current_scope);
      if (if_stmt->alternate) {
        AnalyzeStmt(*if_stmt->alternate, current_scope);
      }
      if (if_stmt->else_block) {
        AnalyzeStmt(*if_stmt->else_block, current_scope);
      }
      break;
    }
    case ASTNode::Kind::kWhileStmt: {
      const auto while_stmt = Cast<ASTWhileStmt>(stmt);
      AnalyzeExpr(while_stmt->condition, current_scope);
      AnalyzeStmt(while_stmt->block, current_scope);
      break;
    }
    case ASTNode::Kind::kReturnStmt: {
      const auto return_stmt = Cast<ASTReturnStmt>(stmt);
      AnalyzeExpr(return_stmt->expr, current_scope);
      break;
    }
    case ASTNode::Kind::kThrowStmt: {
      const auto throw_stmt = Cast<ASTThrowStmt>(stmt);
      AnalyzeExpr(throw_stmt->expr, current_scope);
      break;
    }
    case ASTNode::Kind::kTryStmt: {
      auto try_stmt = Cast<ASTTryStmt>(stmt);
      AnalyzeStmt(try_stmt->try_block, current_scope);
      for (auto& catch_block : try_stmt->catch_blocks) {
        const auto catch_scope =
            zone_->New<Scope>(zone_, Scope::Kind::kCatch, current_scope);
        catch_block.scope = catch_scope;
        if (catch_block.var_name != nullptr &&
            catch_block.var_name->kind() == ASTNode::Kind::kIdentExpr) {
          const auto ident = Cast<ASTIdentExpr>(catch_block.var_name);
          ident->binding = catch_scope->DeclareVariable(
              ident->ident, BindingKind::kLocal, ident->span());
        }
        AnalyzeStmt(catch_block.block, catch_scope);
        catch_scope->ReleaseOwnedSlots();
      }
      for (const auto finally_block : try_stmt->finally_blocks) {
        AnalyzeStmt(finally_block, current_scope);
      }
      break;
    }
    default:
      ABSL_UNREACHABLE();
  }
}

void ScopeAnalyzer::AnalyzeExpr(ZonePtr<ASTExpr> expr, Scope* current_scope) {
  switch (expr->kind()) {
    case ASTNode::Kind::kConstExpr:
      return;
    case ASTNode::Kind::kTemplateExpr: {
      const auto template_expr = Cast<ASTTemplateExpr>(expr);
      for (const auto& segment : template_expr->segments) {
        if (segment.kind == ASTTemplateExpr::Segment::kExpr) {
          AnalyzeExpr(segment.expr_v, current_scope);
        }
      }
      return;
    }
    case ASTNode::Kind::kBinaryExpr: {
      const auto binary_expr = Cast<ASTBinaryExpr>(expr);
      AnalyzeExpr(binary_expr->left, current_scope);
      AnalyzeExpr(binary_expr->right, current_scope);
      return;
    }
    case ASTNode::Kind::kUnaryExpr: {
      const auto unary_expr = Cast<ASTUnaryExpr>(expr);
      AnalyzeExpr(unary_expr->expr, current_scope);
      return;
    }
    case ASTNode::Kind::kGroupExpr: {
      const auto group_expr = Cast<ASTGroupExpr>(expr);
      AnalyzeExpr(group_expr->expr, current_scope);
      return;
    }
    case ASTNode::Kind::kIdentExpr: {
      const auto ident_expr = Cast<ASTIdentExpr>(expr);
      ident_expr->binding = ResolveIdentifier(ident_expr->ident, current_scope);
      return;
    }
    case ASTNode::Kind::kCallExpr: {
      const auto call_expr = Cast<ASTCallExpr>(expr);
      AnalyzeExpr(call_expr->callee, current_scope);
      for (const auto arg : call_expr->args) {
        AnalyzeExpr(arg, current_scope);
      }
      return;
    }
    case ASTNode::Kind::kAssignExpr: {
      const auto assign_expr = Cast<ASTAssignExpr>(expr);
      AnalyzeExpr(assign_expr->target, current_scope);
      AnalyzeExpr(assign_expr->value, current_scope);
      if (assign_expr->target->kind() == ASTNode::Kind::kIdentExpr) {
        const auto target_ident = Cast<ASTIdentExpr>(assign_expr->target);
        if (target_ident->binding != nullptr &&
            target_ident->binding->kind == BindingKind::kGlobal) {
          reporter_->Report(
              Diagnostic::Error(
                  source_file_,
                  absl::StrFormat("Assignment to undeclared global `%s`",
                                  target_ident->ident))
                  .WithLabel(target_ident->span(),
                             "no enclosing module declares this name"));
        }
      }
      return;
    }
    case ASTNode::Kind::kArrayExpr: {
      const auto array_expr = Cast<ASTArrayExpr>(expr);
      AnalyzeExpr(array_expr->target, current_scope);
      for (const auto arg : array_expr->args) {
        AnalyzeExpr(arg, current_scope);
      }
      return;
    }
    case ASTNode::Kind::kNewArrayExpr: {
      const auto new_array_expr = Cast<ASTNewArrayExpr>(expr);
      for (const auto element : new_array_expr->elements) {
        AnalyzeExpr(element, current_scope);
      }
      return;
    }
    case ASTNode::Kind::kNewObjectExpr:
      return;
    case ASTNode::Kind::kMemberAccessExpr: {
      const auto member_access_expr = Cast<ASTMemberAccessExpr>(expr);
      AnalyzeExpr(member_access_expr->expr, current_scope);
      return;
    }
    case ASTNode::Kind::kClosureExpr: {
      const auto closure_expr = Cast<ASTClosureExpr>(expr);
      AnalyzeClosure(closure_expr, current_scope);
      return;
    }
    case ASTNode::Kind::kTupleExpr: {
      const auto tuple_expr = Cast<ASTTupleExpr>(expr);
      for (const auto element : tuple_expr->elements) {
        AnalyzeExpr(element, current_scope);
      }
      return;
    }
    default:
      ABSL_UNREACHABLE();
  }
}
void ScopeAnalyzer::AnalyzePattern(ZonePtr<ASTPattern> pattern,
                                   Scope* current_scope) {
  if (Is<ASTBindingPattern>(pattern)) {
    const auto binding_pattern = static_cast<ASTBindingPattern*>(pattern);

    binding_pattern->binding = current_scope->DeclareVariable(
        binding_pattern->name, BindingKind::kLocal, binding_pattern->span());
  } else if (Is<ASTTuplePattern>(pattern)) {
    const auto tuple_pattern = static_cast<ASTTuplePattern*>(pattern);
    for (const auto ast_pattern : tuple_pattern->patterns) {
      AnalyzePattern(ast_pattern, current_scope);
    }
  }
}

Variable* ScopeAnalyzer::ResolveIdentifier(const std::string_view name,
                                           Scope* current_scope) {
  // First, look through the current function's scope chain (block/catch
  // scopes up to and including the enclosing function scope).
  bool crossed_function = false;
  for (Scope* scope = current_scope; scope != nullptr;
       scope = scope->parent()) {
    if (auto* variable = scope->LookupLocal(name)) {
      if (crossed_function && (variable->kind == BindingKind::kParameter ||
                               variable->kind == BindingKind::kLocal)) {
        if (!variable->captured) {
          variable->captured = true;
          variable->context_slot = scope->AllocateContextSlot();
          scope->set_need_context(true);
        }
      }
      return variable;
    }
    if (scope->kind() == Scope::Kind::kFunction) {
      crossed_function = true;
    }
  }

  // Otherwise, walk module scopes looking for a declared member or a module
  // alias by name. Module scopes are always nested within module or
  // compile-unit scopes (functions/blocks cannot contain modules), so a
  // simple parent walk suffices.
  for (Scope* module_scope = current_scope->EnclosingModule();
       module_scope != nullptr; module_scope = module_scope->parent()) {
    if (module_scope->HasMember(name)) {
      auto* variable = zone_->New<Variable>();
      variable->owning_scope = current_scope;
      variable->name = name;
      variable->kind = BindingKind::kModuleGlobal;
      return variable;
    }
    if (!module_scope->module_name().empty() &&
        module_scope->module_name() == name) {
      auto* variable = zone_->New<Variable>();
      variable->owning_scope = current_scope;
      variable->name = name;
      variable->kind = BindingKind::kModuleGlobal;
      return variable;
    }
  }

  // Fall through: runtime resolution (builtins / pending dynamic globals).
  auto* variable = zone_->New<Variable>();
  variable->name = name;
  variable->kind = BindingKind::kGlobal;
  variable->owning_scope = current_scope;
  return variable;
}

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
