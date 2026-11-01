#include "env.hpp"
#include "util.hpp"

#include <iostream>

namespace env {

HostOs host_os() {
#if defined(_WIN32)
  return HostOs::Windows;
#elif defined(__APPLE__)
  return HostOs::MacOS;
#elif defined(__linux__)
  return HostOs::Linux;
#else
  return HostOs::Unknown;
#endif
}

const char* host_os_name(HostOs os) {
  switch (os) {
    case HostOs::MacOS: return "macos";
    case HostOs::Linux: return "linux";
    case HostOs::Windows: return "windows";
    default: return "unknown";
  }
}

namespace {

bool is_executable(const fs::path& p) {
  std::error_code ec;
  if (!fs::is_regular_file(p, ec)) return false;
#ifdef _WIN32
  return true;
#else
  auto perms = fs::status(p, ec).permissions();
  using fs::perms;
  return (perms & (perms::owner_exec | perms::group_exec | perms::others_exec)) !=
         perms::none;
#endif
}

std::string first_line(std::string s) {
  s = util::trim(s);
  auto nl = s.find('\n');
  if (nl != std::string::npos) s = s.substr(0, nl);
  if (!s.empty() && s.back() == '\r') s.pop_back();
  return s;
}

std::string which_on_path(std::string_view name) {
#ifdef _WIN32
  auto which = util::run({"where", std::string(name)}, {}, true, true);
#else
  auto which = util::run({"which", std::string(name)}, {}, true, true);
#endif
  if (which.exit_code != 0) return {};
  auto line = first_line(which.stdout_str);
  if (!line.empty() && is_executable(line)) return line;
  return {};
}

// Resolve `brew --prefix`.
// macOS Apple Silicon: /opt/homebrew
// macOS Intel / classic: /usr/local
// Linux: ~/.linuxbrew or /home/linuxbrew/.linuxbrew
std::string detect_brew_prefix() {
  std::vector<fs::path> brew_bins;

  if (auto on_path = which_on_path("brew"); !on_path.empty()) {
    brew_bins.push_back(on_path);
  }
  brew_bins.insert(brew_bins.end(), {
      "/opt/homebrew/bin/brew",
      "/usr/local/bin/brew",
      "/home/linuxbrew/.linuxbrew/bin/brew",
  });
  if (const char* home = std::getenv("HOME")) {
    brew_bins.push_back(fs::path(home) / ".linuxbrew" / "bin" / "brew");
  }
#ifdef _WIN32
  brew_bins.push_back("C:/Program Files/Git/usr/bin/brew");
#endif

  for (const auto& brew : brew_bins) {
    if (!is_executable(brew)) continue;
    auto r = util::run({brew.string(), "--prefix"}, {}, true, true);
    if (r.exit_code == 0) {
      auto prefix = first_line(r.stdout_str);
      if (!prefix.empty() && fs::is_directory(prefix)) return prefix;
    }
    if (brew.has_parent_path() && brew.parent_path().filename() == "bin") {
      return brew.parent_path().parent_path().string();
    }
  }

  if (fs::is_directory("/opt/homebrew/bin")) return "/opt/homebrew";
  if (fs::is_directory("/usr/local/Homebrew") ||
      fs::is_directory("/usr/local/Cellar")) {
    return "/usr/local";
  }
  if (fs::is_directory("/home/linuxbrew/.linuxbrew/bin")) {
    return "/home/linuxbrew/.linuxbrew";
  }
  if (const char* home = std::getenv("HOME")) {
    fs::path p = fs::path(home) / ".linuxbrew";
    if (fs::is_directory(p / "bin")) return p.string();
  }
  return {};
}

void push_unique(std::vector<fs::path>& out, const fs::path& p) {
  if (p.empty()) return;
  for (const auto& e : out) {
    if (e == p) return;
  }
  out.push_back(p);
}

std::string find_tool(std::string_view name, const std::string& brew_prefix,
                      HostOs os) {
  if (auto on_path = which_on_path(name); !on_path.empty()) return on_path;

  std::string file(name);
#ifdef _WIN32
  if (file.size() < 4 || file.substr(file.size() - 4) != ".exe") file += ".exe";
#endif

  std::vector<fs::path> candidates;
  if (!brew_prefix.empty()) {
    push_unique(candidates, fs::path(brew_prefix) / "bin" / file);
    push_unique(candidates, fs::path(brew_prefix) / "opt" / "llvm" / "bin" / file);
  }

  if (os == HostOs::MacOS || os == HostOs::Unknown) {
    push_unique(candidates, fs::path("/opt/homebrew/bin") / file);
    push_unique(candidates, fs::path("/opt/homebrew/opt/llvm/bin") / file);
    push_unique(candidates, fs::path("/usr/local/bin") / file);
    push_unique(candidates, fs::path("/usr/local/opt/llvm/bin") / file);
    push_unique(candidates, fs::path("/usr/bin") / file);
  }
  if (os == HostOs::Linux || os == HostOs::Unknown) {
    push_unique(candidates, fs::path("/usr/bin") / file);
    push_unique(candidates, fs::path("/usr/local/bin") / file);
    push_unique(candidates, fs::path("/home/linuxbrew/.linuxbrew/bin") / file);
    if (const char* home = std::getenv("HOME")) {
      push_unique(candidates, fs::path(home) / ".linuxbrew" / "bin" / file);
    }
  }
  if (os == HostOs::Windows || os == HostOs::Unknown) {
    auto envdir = [](const char* key) -> std::string {
      if (const char* v = std::getenv(key)) return v;
      return {};
    };
    std::string pf = envdir("ProgramFiles");
    std::string pf86 = envdir("ProgramFiles(x86)");
    std::string local = envdir("LOCALAPPDATA");
    std::string user = envdir("USERPROFILE");
    std::string choco = envdir("ChocolateyInstall");
    if (pf.empty()) pf = "C:/Program Files";
    if (pf86.empty()) pf86 = "C:/Program Files (x86)";
    if (choco.empty()) choco = "C:/ProgramData/chocolatey";
    push_unique(candidates, fs::path(pf) / "CMake" / "bin" / file);
    push_unique(candidates, fs::path(pf) / "Ninja" / file);
    push_unique(candidates, fs::path(pf) / "LLVM" / "bin" / file);
    push_unique(candidates, fs::path(pf86) / "CMake" / "bin" / file);
    push_unique(candidates, fs::path(pf86) / "LLVM" / "bin" / file);
    push_unique(candidates, fs::path(choco) / "bin" / file);
    if (!user.empty()) {
      push_unique(candidates, fs::path(user) / "scoop" / "shims" / file);
    }
    if (!local.empty()) {
      push_unique(candidates, fs::path(local) / "Microsoft" / "WinGet" / "Links" / file);
    }
  }

  for (const auto& c : candidates) {
    if (is_executable(c)) return c.string();
  }
  return {};
}

ToolInfo check_tool(std::string_view name, const std::string& brew_prefix, HostOs os,
                    std::string_view flag = "--version") {
  ToolInfo t;
  t.path = find_tool(name, brew_prefix, os);
  t.found = !t.path.empty();
  if (t.found) {
    if (auto v = util::command_version(t.path, flag)) t.version = *v;
  }
  return t;
}

} // namespace

Environment detect() {
  Environment e;
  e.os = host_os();
  e.brew_prefix = detect_brew_prefix();
  e.clang   = check_tool("clang", e.brew_prefix, e.os);
  e.clangxx = check_tool("clang++", e.brew_prefix, e.os);
  e.cmake   = check_tool("cmake", e.brew_prefix, e.os);
  e.ninja   = check_tool("ninja", e.brew_prefix, e.os);
  e.git     = check_tool("git", e.brew_prefix, e.os);

  e.cxx = e.clangxx;
  if (!e.cxx.found && e.os != HostOs::Windows) {
    e.cxx = check_tool("g++", e.brew_prefix, e.os);
    if (!e.cxx.found) e.cxx = check_tool("c++", e.brew_prefix, e.os);
  }
  if (!e.cxx.found && e.os == HostOs::Windows) {
    e.cxx = check_tool("clang++", e.brew_prefix, e.os);
    if (!e.cxx.found) e.cxx = check_tool("cl", e.brew_prefix, e.os);
  }
  return e;
}

bool doctor(bool verbose) {
  auto e = detect();
  bool ok = e.cxx.found && e.cmake.found && e.ninja.found;

  auto print = [&](const char* name, const ToolInfo& t, bool required) {
    if (t.found) {
      if (verbose) {
        std::cout << util::green("  ✓ ") << name;
        if (!t.path.empty()) std::cout << "  " << util::dim(t.path);
        if (!t.version.empty()) {
          std::cout << "\n      " << util::dim(t.version);
        }
        std::cout << "\n";
      }
    } else if (verbose) {
      std::cout << (required ? util::red("  ✗ ") : util::dim("  - ")) << name;
      if (required) std::cout << util::red("  [required]");
      else std::cout << util::dim("  [optional]");
      std::cout << "\n";
    }
  };

  if (verbose) {
    std::cout << util::bold("cppup doctor") << " — checking toolchain\n\n";
    std::cout << "  OS        " << util::cyan(host_os_name(e.os)) << "\n";
    if (!e.brew_prefix.empty()) {
      std::cout << "  Homebrew  " << util::cyan(e.brew_prefix) << "\n";
      std::cout << util::dim("             tools searched in ")
                << e.brew_prefix << "/bin\n";
    } else if (e.os == HostOs::MacOS) {
      std::cout << util::dim("  Homebrew  not found")
                << " (checked /opt/homebrew and /usr/local)\n";
    } else if (e.os == HostOs::Linux) {
      std::cout << util::dim("  Homebrew  not found")
                << " (optional; using /usr and PATH)\n";
    } else if (e.os == HostOs::Windows) {
      std::cout << util::dim("  searched   PATH, CMake/LLVM under Program Files,")
                << " scoop, winget, chocolatey\n";
    }
    std::cout << "\n";
  }

  print("c++ compiler", e.cxx, true);
  if (e.cxx.path != e.clangxx.path) print("clang++", e.clangxx, false);
  print("cmake", e.cmake, true);
  print("ninja", e.ninja, true);
  print("clang", e.clang, false);
  print("git", e.git, false);

  if (verbose) {
    std::cout << "\n";
    if (ok) {
      util::success("All required tools found. Ready to build.");
    } else {
      util::error("Missing required tools.");
      std::cout << "\nInstall suggestions:\n";
      std::cout << "  macOS (Apple Silicon, /opt/homebrew):\n"
                << "                  brew install cmake ninja llvm\n";
      std::cout << "  macOS (Intel, /usr/local):\n"
                << "                  brew install cmake ninja llvm\n";
      std::cout << "  Ubuntu/Debian:  sudo apt install clang cmake ninja-build\n";
      std::cout << "  Fedora:         sudo dnf install clang cmake ninja-build\n";
      std::cout << "  Windows:        winget install Kitware.CMake Ninja-build.Ninja LLVM.LLVM\n";
    }
  }
  return ok;
}

bool has_build_tools() {
  auto e = detect();
  return e.cxx.found && e.cmake.found && e.ninja.found;
}

} // namespace env
