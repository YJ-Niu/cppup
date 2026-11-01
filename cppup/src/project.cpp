#include "project.hpp"
#include "templates.hpp"
#include "env.hpp"
#include "util.hpp"

#include <cctype>
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
  std::string exe_name = root.filename().string();
  if (exe_name.empty() || exe_name == "." || exe_name == "/") {
    exe_name = "a.out";
  }

  fs::path exe = build_dir(root) / exe_name;
#ifdef _WIN32
  exe += ".exe";
#endif

  if (!fs::exists(exe)) {
    bool found = false;
    try {
      for (auto& entry : fs::directory_iterator(build_dir(root))) {
        if (entry.is_regular_file()) {
          auto p = entry.path();
          auto perms = fs::status(p).permissions();
          if ((perms & fs::perms::owner_exec) != fs::perms::none) {
            exe = p;
            found = true;
            break;
          }
        }
      }
    } catch (...) {}
    if (!found) {
      util::error("executable not found. Did you build? (cppup build)");
      return false;
    }
  }

  std::vector<std::string> cmd = {exe.string()};
  cmd.insert(cmd.end(), args.begin(), args.end());

  auto r = util::run(cmd, root, false, false);
  return r.exit_code == 0;
}

bool go(const fs::path& root, bool release) {
  if (!build(root, release, true)) return false;
  std::cout << "\n";
  return run(root);
}

bool clean(const fs::path& root) {
  fs::path bdir = build_dir(root);
  if (!fs::exists(bdir)) {
    util::info("nothing to clean (no build/ directory)");
    return true;
  }
  try {
    fs::remove_all(bdir);
    util::success("Removed " + bdir.string());
    return true;
  } catch (const std::exception& e) {
    util::error(std::string("failed to remove build dir: ") + e.what());
    return false;
  }
}

} // namespace project
