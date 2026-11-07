#pragma once

#include <filesystem>

namespace fs = std::filesystem;

namespace check {

struct Options {
  fs::path build_dir;      // compile_commands.json lives here (default: root/build)
  bool format = true;      // clang-format --dry-run --Werror
  bool tidy = true;        // clang-tidy (analyzer, bugprone, unused diagnostics)
  bool fix = false;        // apply clang-format fixes instead of only reporting
  bool verbose = false;
};

// Run the pre-build checks. Returns true when everything passes (or when the
// required tool is unavailable and the check is skipped).
bool run(const fs::path& root, const Options& opts);

} // namespace check
