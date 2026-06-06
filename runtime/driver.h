//
// Created by nothingbutyou on 5/1/26.
//

#ifndef WERSALKALANG_DRIVER_H
#define WERSALKALANG_DRIVER_H

#include <functional>
#include <string_view>
#include <vector>

#include "runtime/ast.h"
#include "runtime/codegen.h"
#include "runtime/diagnostic.h"
#include "runtime/source.h"
#include "runtime/vm/object_impl.h"
#include "runtime/vm/runtime.h"
#include "runtime/vm/value.h"

namespace wersalka {
namespace lang {
namespace runtime {

class Driver;
struct DriverContext;

using ModuleResolverFn = std::function<std::optional<Tagged<ShapedObject>>(
    NativeContext* native_ctx, std::string_view module_name)>;

using ExceptionHandlerFn =
    std::function<void(Runtime* runtime, Value exception)>;

using RuntimeCustomizerFn = std::function<void(Runtime* runtime)>;

struct DriverConfig {
  std::vector<SourceFile> init_files;
  ModuleResolverFn module_resolver;
  ExceptionHandlerFn exception_handler;
  RuntimeCustomizerFn runtime_customizer;
};

struct DriverContext {
  Driver* driver;
  Runtime* runtime;
  DriverConfig* config;
  DiagnosticReporter* reporter;
};

class Driver {
 public:
  struct EnclosingModule {
    std::string_view name;  // empty for anonymous
    GCPtr<ShapedObject> module_object;
  };

  explicit Driver(const DriverConfig& driver_config) : config_(driver_config) {}

  // compile file to anonymous module
  Value CompileFile(const DriverContext* driver_ctx,
                    const NativeContext* native_ctx,
                    const SourceFile* source_file);

  Value CompileModuleOrUnit(const DriverContext* driver_ctx,
                            const NativeContext* native_ctx,
                            ZonePtr<ASTNode> node,
                            const SourceFile* source_file,
                            const std::vector<EnclosingModule>& parent_chain);

  std::optional<Tagged<ShapedObject>> LoadAndCompileModule(
      NativeContext* native_ctx, std::string_view dotted_name,
      const SourceFile* source_file);

  int Execute();

 private:
  using FunctionGeneratorFn = std::function<ZonePtr<CodeObject>(
      ZoneStr name, CodeGenerator& generator)>;

  void InitRuntime(const DriverContext* driver_ctx);
  // TODO: pretty renderer
  static void EmitDiagnostics(const DiagnosticReporter& reporter);
  static GCPtr<FunctionObject> GenerateFunction(const DriverContext* driver_ctx,
                                                std::string_view name,
                                                FunctionGeneratorFn op);

  DriverConfig config_;
};

}  // namespace runtime
}  // namespace lang
}  // namespace wersalka

#endif  // WERSALKALANG_DRIVER_H
