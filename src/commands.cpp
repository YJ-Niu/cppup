#include "commands.hpp"
#include "project.hpp"
#include "env.hpp"
#include "util.hpp"
#include "templates.hpp"

#include <iostream>

namespace commands {

Args parse(int argc, char** argv) {
  Args a;
  if (argc < 2) {
    a.help = true;
    return a;
  }

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      a.help = true;
    } else if (arg == "-f" || arg == "--force") {
      a.force = true;
    } else if (arg == "-r" || arg == "--release") {
      a.release = true;
    } else if (arg == "--verbose") {
      a.verbose = true;
    } else if (arg == "-v" || arg == "--version") {
      a.command = "version";
    } else if (arg == "-t" || arg == "--template") {
      if (i + 1 < argc) {
        a.template_name = argv[++i];
      }
    } else if (arg.starts_with("-")) {
      util::warn("unknown option: " + arg);
    } else {
      if (a.command.empty()) {
        a.command = arg;
      } else {
        a.positional.push_back(arg);
      }
    }
  }
  return a;
}

void print_version() {
  std::cout << "cppup 0.1.0\n";
}

void print_help(std::string_view program) {
  std::cout << util::bold("cppup") << " — one-command C++ project tool\n\n"
            << util::bold("Usage:\n")
            << "  " << program << " <command> [options]\n\n"
            << util::bold("Commands:\n")
            << "  init <name>     Create a new C++23 project\n"
            << "  build           Configure + build with CMake + Ninja\n"
            << "  bundle          Build a distributable app with runtime dependencies\n"
            << "  run             Run the built executable\n"
            << "  go              Build then run (one-shot)\n"
            << "  clean           Remove the build/ directory\n"
            << "  doctor          Check Clang / CMake / Ninja\n"
            << "  templates       List available project templates\n"
            << "  help            Show this help\n"
            << "  version         Show version\n\n"
            << util::bold("Options:\n")
            << "  -v, --version           Show version\n"
            << "  -t, --template <name>   Template for init (default: basic)\n"
            << "  -f, --force             Replace existing output (init or bundle)\n"
            << "  -r, --release           Release build (default: Debug)\n"
            << "      --verbose           More output during build\n"
            << "  -h, --help              Show help\n\n"
            << util::bold("Examples:\n")
            << "  cppup -v\n"
            << "  cppup init MyApp\n"
            << "  cppup init MyApp -t modules\n"
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

  if (args.command == "init") {
    if (args.positional.empty()) {
      util::error("usage: cppup init <name> [-t template] [--force]");
      return 1;
    }
    project::Options opts;
    opts.name = args.positional[0];
    opts.template_name = args.template_name;
    opts.force = args.force;
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
    return project::build(*root, args.release, args.verbose) ? 0 : 1;
  }
  if (args.command == "bundle") {
    return project::bundle(*root, args.release, args.force, args.verbose) ? 0 : 1;
  }
  if (args.command == "run") {
    return project::run(*root, args.positional) ? 0 : 1;
  }
  if (args.command == "go") {
    return project::go(*root, args.release) ? 0 : 1;
  }
  if (args.command == "clean") {
    return project::clean(*root) ? 0 : 1;
  }

  util::error("unknown command: " + args.command);
  std::cout << "Run 'cppup help' for usage.\n";
  return 1;
}

} // namespace commands