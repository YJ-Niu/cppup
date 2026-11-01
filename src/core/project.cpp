#include "core/project.hpp"
#include "templates/templates.hpp"
#include "platform/env.hpp"
#include "util/util.hpp"

#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>

namespace project {

namespace {

// A build/ copied from another machine (or another checkout path) makes
// CMake refuse to run: "CMakeCache.txt directory ... is different".
bool cache_is_foreign(const fs::path& bdir) {
  fs::path cache = bdir / "CMakeCache.txt";
  if (!fs::exists(cache)) return false;
  auto text = util::read_file(cache);
  if (!text) return true;

  const std::string key = "CMAKE_CACHEFILE_DIR:INTERNAL=";
  auto pos = text->find(key);
  if (pos == std::string::npos) return false;
  pos += key.size();
  auto end = text->find('\n', pos);
  auto recorded = util::trim(text->substr(pos, end == std::string::npos
                                                     ? std::string::npos
                                                     : end - pos));
  if (recorded.empty()) return true;

  std::error_code ec;
  auto a = fs::weakly_canonical(fs::path(recorded), ec);
  auto b = fs::weakly_canonical(bdir, ec);
  return a.empty() || b.empty() || a != b;
}

fs::path find_executable(const fs::path& root) {
  std::string exe_name = root.filename().string();
  if (exe_name.empty() || exe_name == "." || exe_name == "/") {
    exe_name = "a.out";
  }

  fs::path exe = build_dir(root) / exe_name;
#ifdef _WIN32
  exe += ".exe";
#endif
  if (fs::exists(exe)) return exe;

  try {
    for (const auto& entry : fs::directory_iterator(build_dir(root))) {
      if (!entry.is_regular_file()) continue;
      const auto path = entry.path();
#ifdef _WIN32
      if (path.extension() == ".exe") return path;
#else
      auto perms = fs::status(path).permissions();
      if ((perms & fs::perms::owner_exec) != fs::perms::none) return path;
#endif
    }
  } catch (...) {}
  return {};
}

std::string cmake_literal(const fs::path& path) {
  const std::string value = path.generic_string();
  std::string equals;
  while (value.find("]" + equals + "]") != std::string::npos) {
    equals += "=";
  }
  return "[" + equals + "[" + value + "]" + equals + "]";
}

bool write_text(const fs::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary);
  out << text;
  return out.good();
}

} // namespace

bool init(const Options& opts) {
  if (opts.name.empty()) {
    util::error("project name is required");
    return false;
  }

  // Validate name (simple)
  for (char c : opts.name) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) {
      util::error("project name may only contain letters, digits, '_' and '-'");
      return false;
    }
  }

  fs::path target = opts.directory.empty()
                        ? fs::current_path() / opts.name
                        : opts.directory;

  if (fs::exists(target)) {
    if (!opts.force) {
      util::error("directory already exists: " + target.string() +
                  "\n  use --force to overwrite (careful!)");
      return false;
    }
    util::warn("overwriting existing directory: " + target.string());
  }

  auto files = templates::generate(opts.template_name, opts.name);
  if (files.empty()) {
    util::error("unknown template: " + opts.template_name);
    std::cout << "Available templates: ";
    auto list = templates::list();
    for (size_t i = 0; i < list.size(); ++i) {
      if (i) std::cout << ", ";
      std::cout << list[i];
    }
    std::cout << "\n";
    return false;
  }

  try {
    fs::create_directories(target);
  } catch (const std::exception& e) {
    util::error(std::string("cannot create directory: ") + e.what());
    return false;
  }

  for (const auto& f : files) {
    fs::path p = target / f.relative_path;
    if (!util::write_file(p, f.content)) {
      util::error("failed to write " + p.string());
      return false;
    }
    std::cout << util::dim("  created ") << f.relative_path << "\n";
  }

  // Optional: git init
  if (util::command_exists("git")) {
    auto r = util::run({"git", "init", "-q"}, target, false, true);
    if (r.exit_code == 0) {
      std::cout << util::dim("  git init") << "\n";
    }
  }

  util::success("Created project '" + opts.name + "' at " + target.string());
  std::cout << "\nNext steps:\n"
            << "  cd " << opts.name << "\n"
            << "  cppup build\n"
            << "  cppup run\n"
            << "  # or simply:  cppup go\n";
  return true;
}

std::optional<fs::path> find_root(const fs::path& start) {
  fs::path cur = fs::absolute(start);
  while (true) {
    if (fs::exists(cur / "CMakeLists.txt")) {
      return cur;
    }
    if (!cur.has_parent_path() || cur == cur.parent_path()) {
      break;
    }
    cur = cur.parent_path();
  }
  return std::nullopt;
}

fs::path build_dir(const fs::path& root) {
  return root / "build";
}

bool build(const fs::path& root, bool release, bool verbose) {
  auto toolchain = env::detect();
  if (!toolchain.cxx.found || !toolchain.cmake.found || !toolchain.ninja.found) {
    util::error("missing build tools. Run 'cppup doctor' for details.");
    return false;
  }

  fs::path bdir = build_dir(root);
  if (cache_is_foreign(bdir)) {
    util::warn("build/ was configured on another path (or another machine).");
    util::info("removing stale CMake cache and reconfiguring");
    std::error_code ec;
    fs::remove_all(bdir, ec);
    if (ec) {
      util::error("failed to remove stale build dir: " + ec.message());
      return false;
    }
  }

  std::string build_type = release ? "Release" : "Debug";

  // Absolute cmake/ninja so a login shell that has not sourced brew
  // (Apple Silicon: /opt/homebrew/bin is often missing from PATH) still works.
  std::vector<std::string> configure = {
      toolchain.cmake.path,
      "-G", "Ninja",
      "-B", bdir.string(),
      "-S", root.string(),
      "-DCMAKE_BUILD_TYPE=" + build_type,
      "-DCMAKE_CXX_COMPILER=" + toolchain.cxx.path,
      "-DCMAKE_MAKE_PROGRAM=" + toolchain.ninja.path,
  };

  if (verbose) {
    std::cout << util::bold("Configuring (") << build_type << ")...\n";
    std::cout << util::dim("  os    ") << env::host_os_name(toolchain.os) << "\n";
    if (!toolchain.brew_prefix.empty()) {
      std::cout << util::dim("  brew  ") << toolchain.brew_prefix << "\n";
    }
    std::cout << util::dim("  cmake ") << toolchain.cmake.path << "\n";
    std::cout << util::dim("  ninja ") << toolchain.ninja.path << "\n";
    std::cout << util::dim("  c++   ") << toolchain.cxx.path << "\n";
  }
  auto conf = util::run(configure, {}, false, !verbose);
  if (conf.exit_code != 0) {
    util::error("cmake configure failed");
    return false;
  }

  std::vector<std::string> build_cmd = {
      toolchain.cmake.path, "--build", bdir.string(),
      "--parallel"
  };
  if (verbose) {
    std::cout << util::bold("Building...\n");
  }
  auto b = util::run(build_cmd, {}, false, !verbose);
  if (b.exit_code != 0) {
    util::error("build failed");
    return false;
  }

  util::success("Build finished (" + build_type + ")");
  return true;
}

bool run(const fs::path& root, const std::vector<std::string>& args) {
  fs::path exe = find_executable(root);
  if (exe.empty()) {
    util::error("executable not found. Did you build? (cppup build)");
    return false;
  }

  std::vector<std::string> cmd = {exe.string()};
  cmd.insert(cmd.end(), args.begin(), args.end());

  auto r = util::run(cmd, root, false, false);
  return r.exit_code == 0;
}

bool bundle(const fs::path& root, bool release, bool force, bool verbose) {
  if (!build(root, release, verbose)) return false;

  const fs::path exe = find_executable(root);
  if (exe.empty()) {
    util::error("executable not found after build");
    return false;
  }

  std::string app_name = root.filename().string();
  if (app_name.empty() || app_name == "." || app_name == "/") {
    app_name = "App";
  }

  const fs::path output_dir = root / "dist";
#ifdef __APPLE__
  const fs::path output = output_dir / (app_name + ".app");
#else
  const fs::path output = output_dir / app_name;
#endif
  const fs::path marker = output / ".cppup-bundle";
  if (fs::exists(output)) {
    if (!force && !fs::exists(marker)) {
      util::error("bundle destination already exists: " + output.string() +
                  " (use --force to replace it)");
      return false;
    }
  }

  fs::path stage;
  try {
    fs::create_directories(output_dir);
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef __APPLE__
    stage = output_dir / ("." + app_name + "-" + std::to_string(nonce) + ".app");
#else
    stage = output_dir / ("." + app_name + "-" + std::to_string(nonce));
#endif
    const fs::path work = build_dir(root) / ".cppup-bundle-work";
    fs::create_directories(work);

#ifdef _WIN32
    const char* cmake_platform = "Windows";
#elif __APPLE__
    const char* cmake_platform = "Darwin";
#elif __linux__
    const char* cmake_platform = "Linux";
#else
    const char* cmake_platform = "Unknown";
#endif

    const fs::path script = work / "bundle.cmake";
    const std::string script_text =
      "set(CMAKE_SYSTEM_NAME " + cmake_literal(cmake_platform) + ")\n"
      "if(CMAKE_SYSTEM_NAME STREQUAL \"Linux\")\n"
      "  set(UNIX TRUE)\n"
      "elseif(CMAKE_SYSTEM_NAME STREQUAL \"Darwin\")\n"
      "  set(UNIX TRUE)\n"
      "  set(APPLE TRUE)\n"
      "  find_program(CMAKE_INSTALL_NAME_TOOL install_name_tool REQUIRED)\n"
      "elseif(CMAKE_SYSTEM_NAME STREQUAL \"Windows\")\n"
      "  set(WIN32 TRUE)\n"
      "endif()\n"
      "include(BundleUtilities)\n"
        "set(BU_COPY_FULL_FRAMEWORK_CONTENTS ON)\n"
        "set(_source " + cmake_literal(exe) + ")\n"
        "set(_stage " + cmake_literal(stage) + ")\n"
        "set(_dirs " + cmake_literal(exe.parent_path()) + ")\n"
        "file(MAKE_DIRECTORY \"${_stage}\")\n"
        "if(APPLE)\n"
        "  file(MAKE_DIRECTORY \"${_stage}/Contents/MacOS\")\n"
        "  file(COPY \"${_source}\" DESTINATION \"${_stage}/Contents/MacOS\")\n"
        "  get_filename_component(_exe_name \"${_source}\" NAME)\n"
        "  file(WRITE \"${_stage}/Contents/Info.plist\" "
        "\"<?xml version=\\\"1.0\\\" encoding=\\\"UTF-8\\\"?>\\n"
        "<!DOCTYPE plist PUBLIC \\\"-//Apple//DTD PLIST 1.0//EN\\\" "
        "\\\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\\\">\\n"
        "<plist version=\\\"1.0\\\"><dict>"
        "<key>CFBundleExecutable</key><string>${_exe_name}</string>"
        "<key>CFBundleIdentifier</key><string>local.cppup.app</string>"
        "<key>CFBundlePackageType</key><string>APPL</string>"
        "</dict></plist>\\n\")\n"
        "  set(_app \"${_stage}/Contents/MacOS/${_exe_name}\")\n"
        "else()\n"
        "  file(COPY \"${_source}\" DESTINATION \"${_stage}\")\n"
        "  get_filename_component(_exe_name \"${_source}\" NAME)\n"
        "  set(_app \"${_stage}/${_exe_name}\")\n"
        "endif()\n"
        "function(gp_resolved_file_type_override resolved_file type_var)\n"
        "  get_filename_component(_name \"${resolved_file}\" NAME)\n"
        "  get_filename_component(_resolved_dir \"${resolved_file}\" DIRECTORY)\n"
        "  if(CMAKE_SYSTEM_NAME STREQUAL \"Linux\" AND _name MATCHES "
        "\"^lib(stdc[+][+]|gcc_s|atomic|gomp|quadmath|omp|c[+][+]|c[+][+]abi|unwind)\\.so\")\n"
        "    if(_resolved_dir STREQUAL _stage)\n"
        "      set(${type_var} local PARENT_SCOPE)\n"
        "    else()\n"
        "      set(${type_var} other PARENT_SCOPE)\n"
        "    endif()\n"
        "  elseif(WIN32 AND _name MATCHES "
        "\"^(msvcp|msvcr|vcruntime|concrt|libstdc[+][+]-6|libgcc_s_.*|libwinpthread-1)"
        "[^/]*\\.dll$\")\n"
        "    if(_resolved_dir STREQUAL _stage)\n"
        "      set(${type_var} local PARENT_SCOPE)\n"
        "    else()\n"
        "      set(${type_var} other PARENT_SCOPE)\n"
        "    endif()\n"
        "  endif()\n"
        "endfunction()\n"
        "fixup_bundle(\"${_app}\" \"\" \"${_dirs}\")\n"
        "file(WRITE \"${_stage}/.cppup-bundle\" \"cppup\\n\")\n";

    if (!write_text(script, script_text)) {
      util::error("failed to write bundle script");
      return false;
    }

    const fs::path cmake_lists = work / "CMakeLists.txt";
    if (!write_text(cmake_lists,
                    "cmake_minimum_required(VERSION 3.20)\n"
                    "project(cppup_bundle NONE)\n"
                    "install(SCRIPT " + cmake_literal(script) + ")\n")) {
      util::error("failed to write bundle project");
      return false;
    }

    auto toolchain = env::detect();
    if (!toolchain.cmake.found) {
      util::error("CMake not found. Run 'cppup doctor' for details.");
      return false;
    }
    auto configure = util::run({toolchain.cmake.path, "-S", work.string(), "-B",
                                (work / "build").string()}, {}, false, !verbose);
    if (configure.exit_code != 0) {
      util::error("failed to configure bundle staging");
      return false;
    }
    auto install = util::run({toolchain.cmake.path, "--install", (work / "build").string(),
                              "--prefix", stage.string()}, {}, false, !verbose);
    if (install.exit_code != 0) {
      std::error_code ec;
      fs::remove_all(stage, ec);
      util::error("failed to collect runtime dependencies");
      return false;
    }

#ifdef __linux__
    fs::rename(stage / exe.filename(), stage / (exe.filename().string() + ".bin"));
    std::ofstream launcher(stage / exe.filename());
    launcher << "#!/bin/sh\n"
                "APP_DIR=$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)\n"
                "APP_NAME=${0##*/}\n"
                "export LD_LIBRARY_PATH=\"$APP_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\"\n"
                "exec \"$APP_DIR/$APP_NAME.bin\" \"$@\"\n";
    if (!launcher.good()) {
      std::error_code ec;
      fs::remove_all(stage, ec);
      util::error("failed to write Linux bundle launcher");
      return false;
    }
    launcher.close();
    fs::permissions(stage / exe.filename(),
                    fs::perms::owner_read | fs::perms::owner_write |
                        fs::perms::owner_exec | fs::perms::group_read |
                        fs::perms::group_exec | fs::perms::others_read |
                        fs::perms::others_exec,
                    fs::perm_options::replace);
#endif

    if (fs::exists(output)) {
      std::error_code ec;
      fs::remove_all(output, ec);
      if (ec) {
        std::error_code cleanup_ec;
        fs::remove_all(stage, cleanup_ec);
        util::error("failed to replace existing bundle: " + ec.message());
        return false;
      }
    }
    fs::rename(stage, output);
    util::success("Bundle created at " + output.string());
    return true;
  } catch (const std::exception& e) {
    std::error_code ec;
    if (!stage.empty()) fs::remove_all(stage, ec);
    util::error(std::string("bundle failed: ") + e.what());
    return false;
  }
}

bool go(const fs::path& root, bool release) {
  if (!build(root, release, true)) return false;
  std::cout << "\n";
  return run(root);
}

bool clean(const fs::path& root) {
  fs::path bdir = build_dir(root);
  try {
    bool removed = false;
    if (fs::exists(bdir)) {
      fs::remove_all(bdir);
      removed = true;
    }

    const fs::path modules_dir = root / "modules";
    if (fs::is_directory(modules_dir)) {
      for (const auto& entry : fs::directory_iterator(modules_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".pcm") {
          fs::remove(entry.path());
          removed = true;
        }
      }
    }

    if (!removed) {
      util::info("nothing to clean (no build/ or module PCM files)");
      return true;
    }
    util::success("Removed build artifacts and module PCM files");
    return true;
  } catch (const std::exception& e) {
    util::error(std::string("failed to clean build artifacts: ") + e.what());
    return false;
  }
}

} // namespace project
