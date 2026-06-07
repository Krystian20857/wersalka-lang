//
// Created by nothingbutyou on 6/6/26.
//

#ifndef WERSALKALANG_SCOPE_H
#define WERSALKALANG_SCOPE_H

#include <string_view>
#include <vector>

#include "runtime/diagnostic.h"
#include "runtime/source.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {
struct ASTClosureExpr;

struct ASTCompileUnit;
struct ASTModuleDecl;
struct ASTFunctionDecl;
struct ASTGlobalDecl;
struct ASTStmt;
struct ASTExpr;
struct ASTPattern;

class Scope;

enum class BindingKind {
  kParameter,        // function-frame slot, declared as function parameter
  kLocal,            // function-frame slot, declared via `var`
  kModuleGlobal,     // declared in some enclosing module or compile unit
  kGlobal,           // unresolved at compile time, falls through to runtime
};

struct Variable : ZoneObject {
  std::string_view name;
  BindingKind kind;
  int slot = -1;          // frame slot, valid for kParameter, kLocal
  int context_slot = -1;  // context slot, valid when captured
  bool captured = false;
  Scope* owning_scope = nullptr;
  TextSpan declaration_span;
};

class Scope : public ZoneObject {
 public:
  enum class Kind { kCompileUnit, kModule, kFunction, kBlock, kCatch };

  Scope(Zone* zone, Kind kind, Scope* parent);

  // Declare a new variable in this scope. For block/catch scopes, the slot is
  // allocated from the enclosing function scope. For module/compile-unit
  // scopes, the variable kind must be kModuleGlobal (no slot). Returns the
  // freshly allocated Variable.
  Variable* DeclareVariable(std::string_view name, BindingKind binding_kind,
                            TextSpan declaration_span);

  // Lookup walks up the scope chain. Returns nullptr if name is not found.
  Variable* Lookup(std::string_view name) const;

  // Lookup limited to this scope only.
  Variable* LookupLocal(std::string_view name) const;

  // Module-scope helpers.
  void AddMember(std::string_view name) { members_.Add(zone_, name); }
  bool HasMember(std::string_view name) const {
    for (const auto member : members_) {
      if (member == name) {
        return true;
      }
    }
    return false;
  }
  void set_module_name(std::string_view name) { module_name_ = name; }
  std::string_view module_name() const { return module_name_; }

  // Function-scope helpers.
  int max_locals() const { return max_user_slots_; }

  // Slot allocation. Block/catch scopes delegate up to their enclosing
  // function scope; function scopes allocate directly.
  int AllocateSlot();
  void ReleaseSlot(int slot);

  int AllocateContextSlot();
  int num_context_slots() const { return num_context_slots_; }

  // Returns all slots used by this scope's locals back to the enclosing
  // function scope (LIFO order). Used by the analyzer when leaving a block.
  void ReleaseOwnedSlots();

  Kind kind() const { return kind_; }
  Scope* parent() const { return parent_; }
  const ZoneList<Variable*>& variables() const { return variables_; }

  // Walks up the parent chain until the nearest enclosing scope of the
  // requested kind. Returns nullptr if none found.
  Scope* EnclosingFunction();
  Scope* EnclosingModule();

  void set_need_context(const bool need_context) { need_context_ = need_context; }
  bool need_context() const { return need_context_; }

 private:
  Zone* zone_;
  Kind kind_;
  Scope* parent_;
  ZoneList<Variable*> variables_;
  ZoneList<std::string_view> members_;
  std::string_view module_name_;

  int next_slot_ = 0;
  int max_user_slots_ = 0;
  ZoneList<int> free_slots_;
  int num_context_slots_ = 0;
  bool need_context_;
};

// Drives the scope-analysis pass. Constructed per compile unit; mutates AST
// nodes in place to attach Variable* / Scope* annotations.
class ScopeAnalyzer {
 public:
  ScopeAnalyzer(Zone* zone, DiagnosticReporter* reporter,
                const SourceFile* source_file);

  // Analyzes a compile unit with an optional chain of enclosing module
  // scopes (outermost first). Returns the compile unit's Scope.
  Scope* AnalyzeCompileUnit(ZonePtr<ASTCompileUnit> unit,
                            const std::vector<Scope*>& enclosing_module_chain);

  Scope* AnalyzeModule(ZonePtr<ASTModuleDecl> module_decl,
                       const std::vector<Scope*>& enclosing_module_chain);

 private:
  void AnalyzeFunction(ZonePtr<ASTFunctionDecl> function_decl,
                       Scope* module_scope);
  void AnalyzeClosure(ZonePtr<ASTClosureExpr> outer_scope,
                       Scope* module_scope);
  void AnalyzeGlobalInit(ZonePtr<ASTGlobalDecl> global_decl,
                         Scope* module_scope);
  void AnalyzeStmt(ZonePtr<ASTStmt> stmt, Scope* current_scope);
  void AnalyzeExpr(ZonePtr<ASTExpr> expr, Scope* current_scope);
  void AnalyzePattern(ZonePtr<ASTPattern> pattern, Scope* current_scope);

  void RegisterMembers(Scope* module_scope,
                       const ZonePtrList<ASTGlobalDecl>& globals,
                       const ZonePtrList<ASTFunctionDecl>& functions,
                       const ZonePtrList<ASTModuleDecl>& inner_modules);

  Variable* ResolveIdentifier(std::string_view name, Scope* current_scope);

  Zone* zone_;
  DiagnosticReporter* reporter_;
  const SourceFile* source_file_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_SCOPE_H
