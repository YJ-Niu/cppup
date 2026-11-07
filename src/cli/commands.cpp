#include "cli/commands.hpp"
#include "core/project.hpp"
#include "core/vcpkg.hpp"
#include "platform/env.hpp"
#include "templates/templates.hpp"
#include "util/util.hpp"
#include "version.h"

#include <iostream>
#include <unordered_map>

namespace commands {

namespace {

enum class Option {
  Help,
  Force,
  Release,
  Verbose,
  Version,
  Template,
  China,
  Official,
  NoCheck,
  Fix,
  Unknown
};

Option parse_option(std::string_view arg) {
  static const std::unordered_map<std::string_view, Option> options = {
      {"-h", Option::Help},       {"--help", Option::Help},
      {"-f", Option::Force},      {"--force", Option::Force},
      {"-r", Option::Release},    {"--release", Option::Release},
      {"--verbose", Option::Verbose},
      {"-v", Option::Version},    {"--version", Option::Version},
      {"-t", Option::Template},   {"--template", Option::Template},
      {"--china", Option::China}, {"--official", Option::Official},
      {"--no-check", Option::NoCheck}, {"--fix", Option::Fix},
  };
  const auto it = options.find(arg);
  return it == options.end() ? Option::Unknown : it->second;
}

} // namespace

Args parse(int argc, char** argv) {
  Args a;
  if (argc < 2) {
    a.help = true;
    return a;
  }

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg.starts_with("-")) {
      switch (parse_option(arg)) {
        case Option::Help:
          a.help = true;
          break;
        case Option::Force:
          a.force = true;
          break;
        case Option::Release:
          a.release = true;
          break;
        case Option::Verbose:
          a.verbose = true;
          break;
        case Option::Version:
          a.command = "version";
          break;
        case Option::Template:
          if (i + 1 < argc) {
            a.template_name = argv[++i];
          } else {
            util::warn("missing value for option: " + std::string(arg));
          }
          break;
        case Option::China:
          a.vcpkg_source = "china";
          break;
        case Option::Official:
          a.vcpkg_source = "official";
          break;
        case Option::NoCheck:
          a.no_check = true;
          break;
        case Option::Fix:
          a.fix = true;
          break;
        case Option::Unknown:
          util::warn("unknown option: " + std::string(arg));
          break;
      }
    } else {
      if (a.command.empty()) {
        a.command = arg;
      } else {
        a.positional.emplace_back(arg);
      }
    }
  }
  return a;
}

void print_version() {
  std::cout << "cppup " << CPPUP_VERSION << "\n";
}

void print_help(std::string_view program) {
  std::cout << util::bold("cppup") << " — one-command C++ project tool\n\n"
            << util::bold("Usage:\n")
            << "  " << program << " <command> [options]\n\n"
            << util::bold("Commands:\n")
            << "  init <name>     Create a new C++23 project (with vcpkg manifest)\n"
            << "  add <module>    Add a local .cppm module to a modules project\n"
            << "  update          Resolve module imports and refresh modules.cmake\n"
            << "  install <port>  Add vcpkg packages and install them\n"
            << "  check           Run format / unused-code / static-analysis checks\n"
            << "  build           Configure + build (runs checks first unless --no-check)\n"
            << "  bundle          Build a distributable app with runtime dependencies\n"
            << "  run             Run the built executable\n"
            << "  go              Build then run (one-shot)\n"
            << "  clean           Remove build output and module PCM files\n"
            << "  vcpkg setup     Install vcpkg (official or China mirror)\n"
            << "  doctor          Check Clang / CMake / Ninja / vcpkg\n"
            << "  templates       List available project templates\n"
            << "  help            Show this help\n"
            << "  version         Show version\n\n"
            << util::bold("Options:\n")
            << "  -v, --version           Show version\n"
            << "  -t, --template <name>   Template for init (default: basic)\n"
            << "  -f, --force             Replace existing output (init/bundle/setup)\n"
            << "  -r, --release           Release build (default: Debug)\n"
            << "      --verbose           More output during build\n"
            << "      --no-check          Skip the code checks before building\n"
            << "      --fix               Apply clang-format and clang-tidy fixes\n"
            << "      --china             Use the China vcpkg mirror (Gitee + domestic assets)\n"
            << "      --official          Use the official GitHub vcpkg source\n"
            << "  -h, --help              Show help\n\n"
            << util::bold("Examples:\n")
            << "  cppup -v\n"
            << "  cppup init MyApp\n"
            << "  cppup init MyApp -t modules\n"
            << "  cppup install fmt\n"
            << "  cppup check --fix\n"
            << "  cppup vcpkg setup --china\n"
            << "  cppup add ../shared/math.cppm\n"
            << "  cppup update\n"
            << "  cd MyApp && cppup go\n"
            << "  cppup doctor\n"
            << "  # first build of cppup itself: ./bootstrap.sh\n"
            << "  # then: ./build/cppup build --release\n";
}

int dispatch(const Args& args) {
  if (args.help || args.command.empty() || args.command == "help") {
    print_help();
    return 0;
  }
  if (args.command == "version") {
    print_version();
    return 0;
  }

  if (args.command == "doctor") {
    return env::doctor(true) ? 0 : 1;
  }

  if (args.command == "templates") {
    std::cout << "Available templates:\n";
    for (const auto& t : templates::list()) {
      std::cout << "  - " << t << "\n";
    }
    return 0;
  }

  if (args.command == "vcpkg") {
    const std::string sub = args.positional.empty() ? "setup" : args.positional[0];
    if (sub != "setup") {
      util::error("usage: cppup vcpkg setup [--china|--official] [--force]");
      return 1;
    }
    if (!args.vcpkg_source.empty()) {
      auto source = *vcpkg::parse_source(args.vcpkg_source);
      return vcpkg::setup(source, args.force, args.verbose) ? 0 : 1;
    }

    const auto existing = vcpkg::locate();
    const auto choice = vcpkg::prompt_setup(existing);
    if (choice.skip) {
      if (existing.found) {
        util::info("skipped the vcpkg installation; using " +
                   existing.root.string());
        std::cout << util::dim(
                         "  switch source with 'cppup vcpkg setup --official' "
                         "or '--china'")
                  << "\n";
      }
      return 0;
    }
    return vcpkg::setup(choice.source, args.force, args.verbose) ? 0 : 1;
  }

  if (args.command == "init") {
    if (args.positional.empty()) {
      util::error("usage: cppup init <name> [-t template] [--china] [--force]");
      return 1;
    }
    project::Options opts;
    opts.name = args.positional[0];
    opts.template_name = args.template_name;
    opts.force = args.force;
    opts.vcpkg_source = args.vcpkg_source;
    return project::init(opts) ? 0 : 1;
  }

  // Commands that need a project root
  auto root = project::find_root();
  if (!root) {
    util::error("not a cppup/CMake project (no CMakeLists.txt found).\n"
                "  Run from inside a project, or create one with 'cppup init <name>'");
    return 1;
  }

  if (args.command == "build") {
    return project::build(*root, args.release, args.verbose, !args.no_check,
                          args.fix)
               ? 0
               : 1;
  }
  if (args.command == "check") {
    return project::check_code(*root, args.release, args.verbose, args.fix) ? 0
                                                                           : 1;
  }
  if (args.command == "install") {
    if (args.positional.empty()) {
      util::info("no packages given; synchronizing the vcpkg manifest");
    }
    return project::install_packages(*root, args.positional) ? 0 : 1;
  }
  if (args.command == "bundle") {
    return project::bundle(*root, args.release, args.force, args.verbose) ? 0 : 1;
  }
  if (args.command == "add") {
    if (args.positional.empty()) {
      util::error("usage: cppup add <path/to/module.cppm>");
      return 1;
    }
    return project::add_module(*root, args.positional.front()) ? 0 : 1;
  }
  if (args.command == "update") {
    return project::update_modules(*root) ? 0 : 1;
  }
  if (args.command == "run") {
    return project::run(*root, args.positional) ? 0 : 1;
  }
  if (args.command == "go") {
    return project::go(*root, args.release, !args.no_check, args.fix) ? 0 : 1;
  }
  if (args.command == "clean") {
    return project::clean(*root) ? 0 : 1;
  }

  util::error("unknown command: " + args.command);
  std::cout << "Run 'cppup help' for usage.\n";
  return 1;
}

} // namespace commands