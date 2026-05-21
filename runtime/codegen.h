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
  explicit CodeGenerator(Runtime* runtime, DiagnosticReporter* reporter)
      : runtime_(runtime), reporter_(reporter) {}

  ZonePtr<CodeObject> CreateCodeObject(ZonePtr<ASTFunctionDecl> ast_func);

 private:
  void CompileStmt(Zone& zone, BytecodeBuilder& builder, LocalsTable& locals,
                   ZonePtr<ASTStmt> stmt);
  void CompileExpr(Zone& zone, BytecodeBuilder& builder, LocalsTable& locals,
                   ZonePtr<ASTExpr> expr);
  void CompileBinaryExpr(Zone& zone, BytecodeBuilder& builder,
                         LocalsTable& locals, ZonePtr<ASTBinaryExpr> expr);
  void CompileUnaryExpr(Zone& zone, BytecodeBuilder& builder,
                        LocalsTable& locals, ZonePtr<ASTUnaryExpr> expr);
  ConstantDesc CompileConstant(BytecodeBuilder& builder, ZonePtr<Token> token);
  std::span<const ConstantDesc> FreezeConstants(Zone* zone,
                                                const BytecodeBuilder& builder);
  std::span<const Instr> FreezeInstructions(Zone* zone,
                                            const BytecodeBuilder& builder);

  Runtime* runtime_;
  DiagnosticReporter* reporter_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_CODEGEN_H
