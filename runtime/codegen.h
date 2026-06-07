//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_CODEGEN_H
#define WERSALKALANG_CODEGEN_H

#include <functional>
#include <span>
#include <vector>

#include "runtime/ast.h"
#include "runtime/diagnostic.h"
#include "runtime/scope.h"
#include "runtime/vm/bytecode.h"
#include "runtime/vm/code_object.h"
#include "runtime/vm/runtime.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

using CodeGenFn = std::function<void()>;

class CodeGenerator {
 public:
  explicit CodeGenerator(
      Runtime* runtime, DiagnosticReporter* reporter, Zone* zone,
      const SourceFile* source_file,
      const Tagged<ShapedObject> current_module =
          Tagged<ShapedObject>(Value::CreateNull()),
      const std::span<const ModuleAlias> ancestor_aliases = {})
      : runtime_(runtime),
        reporter_(reporter),
        zone_(zone),
        source_file_(source_file),
        builder_(zone),
        try_catch_blocks_(zone, 16),
        current_module_(current_module),
        ancestor_aliases_(ancestor_aliases),
        current_scope_(nullptr) {}

  GCPtr<FunctionObject> CompileFunctionObject(
      ZonePtr<ASTFunctionDecl> function_decl);

  GCPtr<FunctionObject> CompileInitObject(
      ZonePtrList<ASTGlobalDecl> globals,
      ZonePtrList<ASTModuleDecl> inner_modules);

  ZonePtr<CodeObject> CompileImportStub(ZoneStr name);

  ZonePtr<CodeObject> CompileCodeObject(ZoneStr debug_name, int params,
                                        int user_max_locals, CodeGenFn op);

 private:
  class ScopeGuard {
   public:
    ScopeGuard(CodeGenerator* generator, Scope* new_scope)
        : generator_(generator), old_scope_(generator->current_scope_) {
      generator->current_scope_ = new_scope;
    }
    ~ScopeGuard() { generator_->current_scope_ = old_scope_; }

   private:
    CodeGenerator* generator_;
    Scope* old_scope_;
  };

  void CompileStmt(ZonePtr<ASTStmt> stmt);
  void CompileExpr(ZonePtr<ASTExpr> expr);
  void CompileBinaryExpr(ZonePtr<ASTBinaryExpr> expr);
  void CompileUnaryExpr(ZonePtr<ASTUnaryExpr> expr);
  void CompileAssignExpr(ZonePtr<ASTAssignExpr> expr);
  void CompileTemplateExpr(ZonePtr<ASTTemplateExpr> expr);
  void CompileLValue(ZonePtr<ASTExpr> target);
  void CompileRValue(ZonePtr<ASTExpr> value);
  void CompileClosureExpr(ZonePtr<ASTClosureExpr> expr);

  void CompileIdent(ZonePtr<ASTIdentExpr> ident, bool is_write);
  void CompileTryStmt(ZonePtr<ASTTryStmt> stmt);
  void CompilePattern(ZonePtr<ASTPattern> pattern, int right_lv);
  ConstantDesc CompileConstant(ZonePtr<Token> token);
  std::span<const ConstantDesc> FreezeConstants(Zone* zone,
                                                const BytecodeBuilder& builder);
  std::span<const Instr> FreezeInstructions(Zone* zone,
                                            const BytecodeBuilder& builder);
  void MarkCurrentLine(ZonePtr<ASTNode> node);
  int AllocateSyntheticSlot();
  int ComputeContextDepth(Scope* declaring);
  int EmitContextEntry(Scope* scope);
  void EmitContextExit(int save_slot);

  struct LoopContext {
    Label continue_label;
    Label break_label;
    int finally_depth;
  };

  Runtime* runtime_;
  DiagnosticReporter* reporter_;
  Zone* zone_;
  const SourceFile* source_file_;
  BytecodeBuilder builder_;
  ZoneList<TryCatchBlock> try_catch_blocks_;
  std::vector<std::function<void()>> finally_blocks_;
  std::vector<LoopContext> loop_stack_;
  Tagged<ShapedObject> current_module_;
  std::span<const ModuleAlias> ancestor_aliases_;
  Scope* current_scope_;

  int next_synthetic_slot_ = 0;
  int max_locals_total_ = 0;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_CODEGEN_H
