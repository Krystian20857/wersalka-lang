//
// Created by nothingbutyou on 5/17/26.
//

#ifndef WERSALKALANG_CODEGEN_H
#define WERSALKALANG_CODEGEN_H

#include "runtime/ast.h"
#include "runtime/diagnostic.h"
#include "runtime/sym.h"
#include "runtime/vm/code_object.h"
#include "runtime/vm/runtime.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

class CodeGenerator {
 public:
  explicit CodeGenerator(Runtime* runtime, DiagnosticReporter* reporter,
                         Zone* zone, const ZonePtr<SourceFile> source_file)
      : runtime_(runtime),
        reporter_(reporter),
        zone_(zone),
        source_file_(source_file),
        builder_(zone),
        try_catch_blocks_(zone, 16) {}

  ZonePtr<CodeObject> CreateCodeObject(ZonePtr<ASTFunctionDecl> ast_func);

 private:
  void CompileStmt(LocalsTable& locals, ZonePtr<ASTStmt> stmt);
  void CompileExpr(LocalsTable& locals, ZonePtr<ASTExpr> expr);
  void CompileBinaryExpr(LocalsTable& locals, ZonePtr<ASTBinaryExpr> expr);
  void CompileUnaryExpr(LocalsTable& locals, ZonePtr<ASTUnaryExpr> expr);
  void CompileAssignExpr(LocalsTable& locals, ZonePtr<ASTAssignExpr> expr);
  void CompileTemplateExpr(LocalsTable& locals, ZonePtr<ASTTemplateExpr> expr);
  void CompileLValue(LocalsTable& locals, ZonePtr<ASTExpr> target);
  void CompileRValue(LocalsTable& locals, ZonePtr<ASTExpr> value);
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
  ZonePtr<SourceFile> source_file_;
  BytecodeBuilder builder_;
  ZoneList<TryCatchBlock> try_catch_blocks_;
  std::vector<std::function<void()>> finally_blocks_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_CODEGEN_H
