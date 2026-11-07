#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vcpkg {

namespace fs = std::filesystem;

// Where vcpkg itself (the tool + port registry) comes from.
enum class Source { Official, ChinaMirror };

std::string source_name(Source source);
std::optional<Source> parse_source(std::string_view text);
// Source recorded in ~/.cppup/config.json, defaults to Official.
Source configured_source();
// Ask on the terminal (defaults to Official when not interactive).
Source prompt_source();

struct Mirror {
  std::string repository;     // git URL of the vcpkg registry
  std::string asset_sources;  // X_VCPKG_ASSET_SOURCES value, empty for official
};

Mirror mirror(Source source);

struct Location {
  bool found = false;
  fs::path root;        // directory containing .vcpkg-root
  fs::path executable;  // vcpkg binary
  std::string version;  // best-effort
};

// Install root used when vcpkg is not configured by the user.
fs::path configured_root();
// Look for vcpkg via VCPKG_ROOT, the cppup config, common locations, or PATH.
Location locate(const fs::path& project_root = {});

struct SetupChoice {
  bool skip = false;  // use the existing installation, do not install
  Source source = Source::Official;
};

// Interactive install choice used by `cppup vcpkg setup` and by build when
// vcpkg is missing. When `existing` is found, a "skip this installation"
// option is offered; otherwise only the source choice is shown.
SetupChoice prompt_setup(const Location& existing);

// Clone + bootstrap vcpkg. May be interactive when source is unset.
bool setup(Source source, bool force = false, bool verbose = false);

// Host triplet used for building/consuming packages.
std::string host_triplet();

bool has_manifest(const fs::path& project_root);
// Create vcpkg.json (and, for the China mirror, vcpkg-configuration.json).
bool ensure_manifest(const fs::path& project_root, std::string_view project_name,
                     Source source);
// Add ports to the manifest and run `vcpkg install` into build/vcpkg_installed.
bool install(const fs::path& project_root, const std::vector<std::string>& packages);

// build/vcpkg_installed/<triplet>
fs::path installed_dir(const fs::path& project_root);
fs::path include_dir(const fs::path& project_root);

// Absolute path of vcpkg's CMake toolchain file, empty when unavailable.
fs::path toolchain_file(const fs::path& project_root);
// Environment variables needed by vcpkg (asset mirror for the China source).
std::vector<std::pair<std::string, std::string>> install_env(
    const fs::path& project_root);

struct ProjectConfig {
  std::string name;
  std::string template_name;
  Source source = Source::Official;
  std::vector<std::string> packages;
};

std::optional<ProjectConfig> read_project_config(const fs::path& project_root);
// Write .cppup.json with the project's vcpkg metadata.
bool write_project_config(const fs::path& project_root,
                          const ProjectConfig& config);

} // namespace vcpkg
