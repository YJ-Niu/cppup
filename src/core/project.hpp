#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <optional>

namespace fs = std::filesystem;

namespace project {

struct Options {
  std::string name;
  std::string template_name = "basic";
  fs::path    directory;          // where to create (default = cwd / name)
  std::string vcpkg_source;       // "official", "china", or empty (= config)
  bool        force = false;      // overwrite existing
};

// Create a new project. Returns true on success.
bool init(const Options& opts);

// Install vcpkg ports into the current project (manifest mode).
bool install_packages(const fs::path& root,
                      const std::vector<std::string>& packages);

// Add a local C++ module interface and its referenced module sources.
bool add_module(const fs::path& root, const fs::path& source);

// Resolve imports for all module interfaces in modules/ and regenerate the manifest.
bool update_modules(const fs::path& root);

// Find project root by walking up looking for CMakeLists.txt
std::optional<fs::path> find_root(const fs::path& start = fs::current_path());

// Build directory (root / "build")
fs::path build_dir(const fs::path& root);

// Configure + build with CMake + Ninja. By default code format, unused-code
// and static-analysis checks run after configure and before compiling; the
// build stops when any check fails.
bool build(const fs::path& root, bool release = false, bool verbose = false,
           bool run_checks = true, bool fix = false);

// Configure (to generate compile_commands.json) and run the code checks
// without building.
bool check_code(const fs::path& root, bool release = false, bool verbose = false,
                bool fix = false);

bool bundle(const fs::path& root, bool release = false, bool force = false,
            bool verbose = false);

// Run the executable (assumes already built)
bool run(const fs::path& root, const std::vector<std::string>& args = {});

// Build then run
bool go(const fs::path& root, bool release = false, bool run_checks = true,
        bool fix = false);

// Remove build directory
bool clean(const fs::path& root);

} // namespace project