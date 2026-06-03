//
// Created by nothingbutyou on 6/3/26.
//

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/parse.h"
#include "runtime/driver.h"

int main(int argc, char** argv) {
  absl::InitializeSymbolizer(argv[0]);
  absl::InstallFailureSignalHandler({});
  const auto positional_args = absl::ParseCommandLine(argc, argv);
  std::vector<std::string_view> files(positional_args.begin() + 1,
                                      positional_args.end());
  wersalka::lang::runtime::Driver driver;
  return driver.RunFiles(files);
}
