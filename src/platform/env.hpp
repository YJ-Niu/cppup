#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace fs = std::filesystem;

namespace env {

enum class HostOs { MacOS, Linux, Windows, Unknown };

HostOs host_os();
const char* host_os_name(HostOs os);

struct ToolInfo {
  bool found = false;
  std::string path;          // absolute path when resolved
  std::string version;       // first line of --version
};

struct Environment {
  HostOs os = HostOs::Unknown;
  ToolInfo clang;
  ToolInfo clangxx;
  ToolInfo cxx;              // compiler actually used (clang++, else g++/cl)
  ToolInfo cmake;
  ToolInfo ninja;
  ToolInfo git;              // optional but nice
  std::string brew_prefix;   // empty if Homebrew is not installed
};

Environment detect(bool prefer_homebrew_llvm = false);

// Print human-readable report. Returns true if all required tools are present.
bool doctor(bool verbose = true);

// Required tools for build
bool has_build_tools();

} // namespace env
