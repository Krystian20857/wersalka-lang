//
// Created by nothingbutyou on 6/3/26.
//

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_replace.h"
#include "runtime/driver.h"
#include "vm/vm_intrinsics.h"

int main(int argc, char** argv) {
  namespace runtime = wersalka::lang::runtime;
  namespace fs = std::filesystem;

  absl::InitializeSymbolizer(argv[0]);
  absl::InstallFailureSignalHandler({});
  const auto positional_args = absl::ParseCommandLine(argc, argv);
  std::vector<std::string> files(positional_args.begin() + 1,
                                 positional_args.end());
  std::vector<runtime::SourceFile> source_files;
  for (auto file : files) {
    std::ifstream fstream(file);
    if (!fstream) {
      std::cerr << "Source file not found: " << file << std::endl;
      return 1;
    }
    std::string content(std::istreambuf_iterator(fstream), {});
    source_files.push_back(
        runtime::SourceFile(content, fs::path(file).stem().string()));
  }

  std::vector<fs::path> module_search_paths;
  for (const auto& file : files) {
    auto dir = fs::path(file).parent_path();
    if (dir.empty()) {
      dir = fs::current_path();
    }
    if (std::find(module_search_paths.begin(), module_search_paths.end(),
                  dir) == module_search_paths.end()) {
      module_search_paths.push_back(dir);
    }
  }
  module_search_paths.push_back(fs::current_path());

  std::list<runtime::SourceFile> imported_sources;

  const runtime::DriverConfig driver_config{
      .init_files = source_files,
      .module_resolver = [&module_search_paths, &imported_sources](
                             runtime::NativeContext* native_ctx,
                             std::string_view module_name)
          -> std::optional<runtime::Tagged<runtime::ShapedObject>> {
        // TODO: rethink this resolver
        const auto relative =
            fs::path(absl::StrReplaceAll(module_name, {{".", "/"}}))
                .replace_extension(".w");
        for (const auto& root : module_search_paths) {
          const auto candidate = root / relative;
          std::ifstream fstream(candidate);
          if (!fstream) {
            continue;
          }
          std::string content(std::istreambuf_iterator(fstream), {});
          imported_sources.emplace_back(content);
          const auto driver_ctx =
              static_cast<runtime::DriverContext*>(native_ctx->runtime->ctx);
          return driver_ctx->driver->LoadAndCompileModule(
              native_ctx, module_name, &imported_sources.back());
        }
        return std::nullopt;
      },
      .exception_handler =
          [](auto runtime, auto exception) {
            std::cerr << "Runtime error: "
                      << runtime::VMIntrinsics::ToString(runtime, exception)
                      << std::endl;
          },
      .runtime_customizer =
          [](auto runtime) {
            // noop
          }};
  runtime::Driver driver(driver_config);
  return driver.Execute();
}
