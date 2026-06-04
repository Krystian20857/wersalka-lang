//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_CODEGEN_H
#define WERSALKALANG_CODEGEN_H

#include "absl/container/flat_hash_set.h"
#include "runtime/ast.h"
#include "runtime/diagnostic.h"
#include "runtime/sym.h"
#include "runtime/vm/code_object.h"
#include "runtime/vm/runtime.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

struct ModuleScope {
  std::string_view name;
  absl::flat_hash_set<std::string_view> members;
};

using CodeGenFn = std::function<void(LocalsTable& locals)>;

class CodeGenerator {
 public:
  explicit CodeGenerator(Runtime* runtime, DiagnosticReporter* reporter,
                         Zone* zone, const SourceFile* source_file,
                         std::vector<ModuleScope> lexical_scopes)
      : runtime_(runtime),
        reporter_(reporter),
        zone_(zone),
        source_file_(source_file),
        builder_(zone),
        try_catch_blocks_(zone, 16),
        lexical_scopes_(std::move(lexical_scopes)) {}

  GCPtr<FunctionObject> CompileFunctionObject(
      ZonePtr<ASTFunctionDecl> function_decl);

  GCPtr<FunctionObject> CompileInitObject(
      ZonePtrList<ASTGlobalDecl> globals,
      ZonePtrList<ASTModuleDecl> inner_modules);

  ZonePtr<CodeObject> CompileImportStub(ZoneStr name);

  ZonePtr<CodeObject> CompileCodeObject(ZoneStr debug_name, int params,
                                        CodeGenFn op);

 private:
  void CompileStmt(LocalsTable& locals, ZonePtr<ASTStmt> stmt);
  void CompileExpr(LocalsTable& locals, ZonePtr<ASTExpr> expr);
  void CompileBinaryExpr(LocalsTable& locals, ZonePtr<ASTBinaryExpr> expr);
  void CompileUnaryExpr(LocalsTable& locals, ZonePtr<ASTUnaryExpr> expr);
  void CompileAssignExpr(LocalsTable& locals, ZonePtr<ASTAssignExpr> expr);
  void CompileTemplateExpr(LocalsTable& locals, ZonePtr<ASTTemplateExpr> expr);
  void CompileLValue(LocalsTable& locals, ZonePtr<ASTExpr> target);
  void CompileRValue(LocalsTable& locals, ZonePtr<ASTExpr> value);

  void CompileIdent(LocalsTable& locals, std::string_view name, bool is_write,
                    TextSpan span);
  void CompileTryStmt(LocalsTable& locals, ZonePtr<ASTTryStmt> stmt);
  ConstantDesc CompileConstant(ZonePtr<Token> token);
  std::span<const ConstantDesc> FreezeConstants(Zone* zone,
                                                const BytecodeBuilder& builder);
  std::span<const Instr> FreezeInstructions(Zone* zone,
                                            const BytecodeBuilder& builder);
  void MarkCurrentLine(ZonePtr<ASTNode> node);

  Runtime* runtime_;
  DiagnosticReporter* reporter_;
  Zone* zone_;
  const SourceFile* source_file_;
  BytecodeBuilder builder_;
  ZoneList<TryCatchBlock> try_catch_blocks_;
  std::vector<std::function<void()>> finally_blocks_;
  std::vector<ModuleScope> lexical_scopes_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_CODEGEN_H
