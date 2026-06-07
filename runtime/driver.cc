//
// Created by nothingbutyou on 5/1/26.
//

#include "runtime/driver.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "absl/strings/str_format.h"
#include "runtime/codegen.h"
#include "runtime/diagnostic.h"
#include "runtime/parser.h"
#include "runtime/scope.h"
#include "runtime/source.h"
#include "runtime/tokenizer.h"
#include "runtime/vm/vm.h"
#include "runtime/zone.h"

namespace wersalka {
namespace lang {
namespace runtime {

Value Driver::CompileFile(const DriverContext* driver_ctx,
                          const NativeContext* native_ctx,
                          const SourceFile* source_file) {
  const auto runtime = driver_ctx->runtime;
  const auto reporter = driver_ctx->reporter;
  Zone zone;
  Tokenizer tokenizer(&zone, reporter, source_file);
  Parser parser(&zone, &tokenizer, reporter, source_file);
  const auto ast = parser.Parse();
  if (reporter->HasError()) {
    // TODO: error -> exception adapter
    // native_ctx->exception
    return Value::CreateNull();
  }
  if (Is<ASTCompileUnit>(ast)) {
    ScopeAnalyzer analyzer(&zone, reporter, source_file);
    analyzer.AnalyzeCompileUnit(Cast<ASTCompileUnit>(ast), {});
    if (reporter->HasError()) {
      return Value::CreateNull();
    }
    std::vector<EnclosingModule> empty_chain;
    return CompileModuleOrUnit(driver_ctx, native_ctx,
                               Cast<ASTCompileUnit>(ast), source_file,
                               empty_chain);
  } else {
    *native_ctx->exception = runtime->NewException("Invalid AST");
    return Value::CreateNull();
  }
}
Value Driver::CompileModuleOrUnit(
    const DriverContext* driver_ctx, const NativeContext* native_ctx,
    const ZonePtr<ASTNode> node, const SourceFile* source_file,
    const std::vector<EnclosingModule>& parent_chain) {
  CHECK(node->kind() == ASTNode::Kind::kModuleDecl ||
        node->kind() == ASTNode::Kind::kCompileUnit);
  // TODO: maybe make module/unit body somehow shared?
  Zone zone;
  ZonePtrList<ASTFunctionDecl> functions(&zone);
  ZonePtrList<ASTGlobalDecl> globals(&zone);
  ZonePtrList<ASTModuleDecl> inner_modules(&zone);
  Tagged<StringObject> module_name;
  bool anonymous = false;
  if (Is<ASTModuleDecl>(node)) {
    const auto ast_module = Cast<ASTModuleDecl>(node);
    functions.AddAll(&zone, &ast_module->functions);
    globals.AddAll(&zone, &ast_module->globals);
    inner_modules.AddAll(&zone, &ast_module->inner_modules);
    module_name =
        StringObject::New(driver_ctx->runtime->gc(), ast_module->name);
  } else if (Is<ASTCompileUnit>(node)) {
    const auto ast_compile_unit = Cast<ASTCompileUnit>(node);
    functions.AddAll(&zone, &ast_compile_unit->functions);
    globals.AddAll(&zone, &ast_compile_unit->globals);
    inner_modules.AddAll(&zone, &ast_compile_unit->inner_modules);
    module_name = StringObject::New(
        driver_ctx->runtime->gc(),
        absl::StrFormat(
            "__anonymous_%d",
            VMIntrinsics::IdentityHash(driver_ctx->runtime, source_file)));
    anonymous = true;
  }

  auto runtime = driver_ctx->runtime;
  auto reporter = driver_ctx->reporter;
  const auto perm_zone = runtime->GetPermanentZone();
  const auto module_object =
      ShapedObject::New(runtime->gc(), runtime->shaped_tree()->root());

  // metadata
  VMIntrinsics::SetField(native_ctx,
                         Value::CreateObject(module_object),  // object
                         Value::CreateObject(StringObject::New(
                             runtime->gc(), "__metadata")),  // field name
                         Value::CreateObject(ModuleMetaObject::New(
                             runtime->gc(), module_name, anonymous))  // value
  );

  const auto current_name_interned =
      anonymous ? std::string_view{}
                : perm_zone->InternString(module_name->ToStringView());

  int alias_count = 0;
  if (!anonymous) {
    alias_count++;
  }
  for (const auto& parent : parent_chain) {
    if (!parent.name.empty()) {
      alias_count++;
    }
  }
  std::span<const ModuleAlias> aliases;
  if (alias_count > 0) {
    auto* buffer = perm_zone->NewBuffer<ModuleAlias>(alias_count);
    int idx = 0;
    if (!anonymous) {
      buffer[idx++] = ModuleAlias{current_name_interned, module_object};
    }
    for (auto it = parent_chain.rbegin(); it != parent_chain.rend(); ++it) {
      if (!it->name.empty()) {
        buffer[idx++] = ModuleAlias{it->name, it->module_object};
      }
    }
    aliases = std::span<const ModuleAlias>(buffer, alias_count);
  }

  // functions
  for (const auto function : functions) {
    Zone codegen_zone;
    CodeGenerator code_generator(runtime, reporter, &codegen_zone, source_file,
                                 module_object, aliases);
    const auto function_object = code_generator.CompileFunctionObject(function);
    if (reporter->HasError()) {
      // TODO: error -> exception adapter
      // native_ctx->exception
      return Value::CreateNull();
    }
    VMIntrinsics::SetField(
        native_ctx,
        Value::CreateObject(module_object),  // object
        Value::CreateObject(StringObject::New(
            runtime->gc(), function_object->name())),  // field name
        Value::CreateObject(function_object)           // value
    );
  }

  // init block
  Zone codegen_zone;
  CodeGenerator code_generator(runtime, reporter, &codegen_zone, source_file,
                               module_object, aliases);
  const auto init_function_object =
      code_generator.CompileInitObject(globals, inner_modules);
  VMIntrinsics::SetField(
      native_ctx,
      Value::CreateObject(module_object),  // object
      Value::CreateObject(StringObject::New(
          runtime->gc(), init_function_object->name())),  // field name
      Value::CreateObject(init_function_object)           // value
  );

  // extend the chain for inner modules.
  std::vector<EnclosingModule> extended_chain = parent_chain;
  extended_chain.push_back(
      EnclosingModule{current_name_interned, module_object});

  // inner modules
  for (auto inner_module : inner_modules) {
    const auto inner_module_object = CompileModuleOrUnit(
        driver_ctx, native_ctx, inner_module, source_file, extended_chain);
    VMIntrinsics::SetField(
        native_ctx,
        Value::CreateObject(module_object),  // object
        Value::CreateObject(StringObject::New(
            runtime->gc(), inner_module->name)),  // field name
        inner_module_object                       // value
    );
  }

  return Value::CreateObject(module_object);
}
std::optional<Tagged<ShapedObject>> Driver::LoadAndCompileModule(
    NativeContext* native_ctx, const std::string_view dotted_name,
    const SourceFile* source_file) {
  const auto driver_ctx = static_cast<DriverContext*>(native_ctx->runtime->ctx);
  CHECK(driver_ctx != nullptr);
  CHECK(driver_ctx->driver == this);

  const auto compiled = CompileFile(driver_ctx, native_ctx, source_file);
  if (driver_ctx->reporter->HasError()) {
    return std::nullopt;
  }
  if (compiled.IsNull() || !compiled.IsObject(ObjectKind::kShapedObject)) {
    return std::nullopt;
  }

  const auto runtime = driver_ctx->runtime;
  const auto module = compiled.GetObjectUnchecked<ShapedObject>();
  VMIntrinsics::SetField(
      native_ctx, compiled,
      Value::CreateObject(StringObject::New(runtime->gc(), "__metadata")),
      Value::CreateObject(ModuleMetaObject::New(
          runtime->gc(), StringObject::New(runtime->gc(), dotted_name),
          /*anonymous=*/false)));

  return Tagged<ShapedObject>(module);
}

int Driver::Execute() {
  Zone runtime_zone;
  Runtime runtime(&runtime_zone, nullptr);
  DiagnosticReporter reporter;
  DriverContext driver_ctx(this, &runtime, &config_, &reporter);
  runtime.ctx = &driver_ctx;
  InitRuntime(&driver_ctx);

  auto pending_exception = Value::CreateNull();
  NativeContext native_ctx(&runtime, &pending_exception);

  VMInterpreter interpreter(&runtime);
  const auto main_thread = std::make_unique<VMThread>(&runtime);

  for (auto& init_file : config_.init_files) {
    const auto compiled_module =
        CompileFile(&driver_ctx, &native_ctx, &init_file);
    if (reporter.HasError()) {
      EmitDiagnostics(reporter);
      return 1;
    }
    if (!compiled_module.IsNull() &&
        compiled_module.IsObject(ObjectKind::kShapedObject)) {
      if (!init_file.module_name().empty()) {
        VMIntrinsics::SetField(
            &native_ctx, compiled_module,
            Value::CreateObject(StringObject::New(runtime.gc(), "__metadata")),
            Value::CreateObject(ModuleMetaObject::New(
                runtime.gc(),
                StringObject::New(runtime.gc(), init_file.module_name()),
                /*anonymous=*/false)));
      }
      runtime.AddRootModule(compiled_module);

      const auto init_fn_name =
          StringObject::New(runtime.gc(), "__module_init");
      const auto init_fn =
          VMIntrinsics::GetField(&native_ctx, compiled_module, init_fn_name);
      if (init_fn.IsObject(ObjectKind::kFunction)) {
        const Value init_args[] = {compiled_module};
        interpreter.Execute(main_thread.get(),
                            init_fn.GetObjectUnchecked<FunctionObject>(),
                            init_args);
        if (main_thread->GetThreadState() == VMThreadState::kError) {
          config_.exception_handler(&runtime,
                                    main_thread->GetCurrentException());
          return 1;
        }
      }
    }
  }

  const auto main_str = StringObject::New(runtime.gc(), "__main__");
  std::optional<Value> main_fn = std::nullopt;
  for (const auto root_module : runtime.root_modules()) {
    const auto main_value = VMIntrinsics::GetField(
        &native_ctx, root_module, Value::CreateObject(main_str));
    if (!main_value.IsNull() && main_value.IsObject(ObjectKind::kFunction)) {
      main_fn = main_value;
      break;
    }
  }

  if (!main_fn) {
    config_.exception_handler(
        &runtime, runtime.NewException(
                      "error: no '__main__' function defined in root modules"));
    return 1;
  }

  interpreter.Execute(main_thread.get(),
                      main_fn->GetObjectUnchecked<FunctionObject>());

  if (main_thread->GetThreadState() == VMThreadState::kError) {
    config_.exception_handler(&runtime, main_thread->GetCurrentException());
    return 1;
  }
  return 0;
}
void Driver::InitRuntime(const DriverContext* driver_ctx) {
  const auto runtime = driver_ctx->runtime;
  runtime->builtins()->RegisterBuiltIns();
  runtime->BindGlobalFunction("print", 1, [](auto ctx, auto args) {
    std::cout << VMIntrinsics::ToString(ctx->runtime, args[0])
              << std::endl;  // flush immediately
    return Value::CreateNull();
  });
  runtime->BindGlobalFunction("assert", 2, [](auto ctx, auto args) {
    const auto condition = args[0];
    const auto message = args[1];
    if (!VMIntrinsics::IsTruthful(condition)) {
      *ctx->exception = ctx->runtime->NewException(
          absl::StrFormat("Assertion failed: %s",
                          VMIntrinsics::ToString(ctx->runtime, message)));
    }
    return Value::CreateNull();
  });
  runtime->BindGlobalFunction(
      "__load_module", 1, [](NativeContext* ctx, std::span<const Value> args) {
        const auto module_name_value = args[0];
        if (!module_name_value.IsObject(ObjectKind::kString)) {
          *ctx->exception =
              ctx->runtime->NewException("Invalid module name argument");
          return Value::CreateNull();
        }

        // TODO: use handle scope here
        const auto module_name =
            module_name_value.template GetObjectUnchecked<StringObject>();
        const auto dotted_name = module_name->ToStringView();
        const auto metadata_field_name =
            StringObject::New(ctx->runtime->gc(), "__metadata");

        const auto is_module_value = [&](Value value) {
          if (!value.IsObject(ObjectKind::kShapedObject)) {
            return false;
          }
          const auto md =
              VMIntrinsics::GetField(ctx, value, metadata_field_name);
          return md.IsObject(ObjectKind::kModuleMeta);
        };

        const auto descend = [&](Value start) -> Value {
          Value current = start;
          std::string_view remaining = dotted_name;
          while (!remaining.empty()) {
            const auto dot = remaining.find('.');
            const auto segment = remaining.substr(0, dot);
            const auto segment_value = Value::CreateObject(
                StringObject::New(ctx->runtime->gc(), segment));
            const auto next =
                VMIntrinsics::GetField(ctx, current, segment_value);
            if (!is_module_value(next)) {
              return Value::CreateNull();
            }
            current = next;
            if (dot == std::string_view::npos) {
              break;
            }
            remaining = remaining.substr(dot + 1);
          }
          return current;
        };

        // pass 1: prefer descended (inner-module) matches. A file whose
        // wrapper is named "math" but also declares `module math { ... }`
        // inside should resolve `import("math")` to the inner declaration
        for (const auto root_module : ctx->runtime->root_modules()) {
          const auto descended = descend(root_module);
          if (!descended.IsNull()) {
            return descended;
          }
        }
        // pass 2: direct match by metadata.name (catches wrappers stamped
        // by `LoadAndCompileModule` or by init-file naming).
        for (const auto root_module : ctx->runtime->root_modules()) {
          const auto metadata_field =
              VMIntrinsics::GetField(ctx, root_module, metadata_field_name);
          const auto metadata =
              metadata_field.template GetObjectUnchecked<ModuleMetaObject>();
          if (!metadata->anonymous() &&
              metadata->name()->ToStringView() == dotted_name) {
            return root_module;
          }
        }

        // fallback to module resolver
        const auto driver_ctx = static_cast<DriverContext*>(ctx->runtime->ctx);
        const auto module = driver_ctx->config->module_resolver(
            ctx, module_name->ToStringView());
        if (module) {
          const auto module_value = Value::CreateObject(module->Get());
          ctx->runtime->AddRootModule(module_value);
          return module_value;
        }
        return Value::CreateNull();
      });
  runtime->RegisterFunction(
      GenerateFunction(driver_ctx, "import", [](auto name, auto& generator) {
        return generator.CompileImportStub(name);
      }));
  driver_ctx->config->runtime_customizer(runtime);
}
void Driver::EmitDiagnostics(const DiagnosticReporter& reporter) {
  for (const auto& diagnostic : reporter.diagnostics()) {
    const char* severity =
        (diagnostic.severity == Severity::kError) ? "error" : "warning";
    std::cerr << absl::StrFormat("[%s] %s\n", severity, diagnostic.message);
    for (const auto& label : diagnostic.labels) {
      const auto location =
          diagnostic.source_file->LocationOf(label.span.offset);
      std::cerr << absl::StrFormat("  --> [%d:%d] %s\n", location.line,
                                   location.column, label.message);
    }
    for (const auto& note : diagnostic.notes) {
      const char* kind = (note.severity == Severity::kHelp) ? "help" : "note";
      std::cerr << absl::StrFormat("  = %s: %s\n", kind, note.message);
    }
  }
}
GCPtr<FunctionObject> Driver::GenerateFunction(const DriverContext* driver_ctx,
                                               std::string_view name,
                                               FunctionGeneratorFn op) {
  const auto runtime = driver_ctx->runtime;
  Zone codegen_zone;
  CodeGenerator code_generator(runtime, driver_ctx->reporter, &codegen_zone,
                               &runtime->builtins()->dummy_source_file(),
                               Value::CreateNull(), {});
  const auto interned_name = runtime->GetPermanentZone()->InternString(name);
  return driver_ctx->runtime->gc()->New<FunctionObject>(
      interned_name, op(interned_name, code_generator));
}
}  // namespace runtime
}  // namespace lang
}  // namespace wersalka
