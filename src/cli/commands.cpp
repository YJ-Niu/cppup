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
  std::cout
      << util::bold("cppup") << " — 一站式 C++ 项目管理工具\n\n"
      << util::bold("用法:\n")
      << "  " << program << " <命令> [参数]\n"
      << "  在项目根目录或其任意子目录执行；根目录通过向上查找 CMakeLists.txt 确定\n\n"

      << util::bold("命令:\n")
      << "  init <项目名>                 创建新的 C++23 项目（自动初始化 vcpkg 清单）\n"
      << "      -t, --template <名称>     选择模板：basic（默认） | modules\n"
      << "      -f, --force               覆盖已存在的项目目录（会删除原有内容）\n"
      << "      --china / --official      指定本项目使用的 vcpkg 源\n"
      << "      示例: cppup init MyApp\n"
      << "            cppup init MyLib -t modules\n"
      << "            cppup init Demo --china --force\n\n"

      << "  add <模块.cppm>               向 modules 项目添加本地模块\n"
      << "      源文件需包含 export module <名称>;，本地依赖会被一起复制\n"
      << "      示例: cppup add ../shared/math.cppm\n\n"

      << "  update                        重新扫描 modules/ 并刷新 modules/modules.cmake\n"
      << "      示例: cppup update\n\n"

      << "  install <包名>...             通过 vcpkg 添加并安装依赖，写入项目配置\n"
      << "      不带包名时仅同步 vcpkg.json 中的依赖\n"
      << "      示例: cppup install fmt\n"
      << "            cppup install fmt spdlog\n"
      << "            cppup install\n\n"

      << "  check                         运行代码检查（格式 / 未使用代码 / 内存安全）\n"
      << "      --fix                     自动应用 clang-format 与 clang-tidy 修复\n"
      << "      示例: cppup check\n"
      << "            cppup check --fix\n\n"

      << "  build                         配置并构建项目（默认 Debug，构建前先运行检查）\n"
      << "      -r, --release             使用 Release 构建\n"
      << "      --no-check                跳过构建前的代码检查\n"
      << "      --fix                     检查时自动修复可修复的问题\n"
      << "      --verbose                 输出工具链与构建的详细过程\n"
      << "      有 vcpkg.json 时自动使用 vcpkg；已安装则跳过安装，未安装则询问安装源\n"
      << "      示例: cppup build\n"
      << "            cppup build --release\n"
      << "            cppup build --release --no-check\n"
      << "            cppup build --fix --verbose\n\n"

      << "  bundle                        构建并收集非系统运行时依赖到 dist/\n"
      << "      -r, --release             使用 Release 构建\n"
      << "      -f, --force               替换已存在的输出目录\n"
      << "      示例: cppup bundle --release --force\n\n"

      << "  run [程序参数...]             运行已构建的可执行文件，参数原样传递给程序\n"
      << "      示例: cppup run\n"
      << "            cppup run --help\n\n"

      << "  go                            一步完成构建并运行\n"
      << "      -r, --release             使用 Release 构建\n"
      << "      --no-check                跳过构建前的代码检查\n"
      << "      --fix                     检查时自动修复可修复的问题\n"
      << "      示例: cppup go\n"
      << "            cppup go --release\n\n"

      << "  clean                         删除 build/ 与生成的模块 PCM 文件（保留源码）\n"
      << "      示例: cppup clean\n\n"

      << "  vcpkg setup                   安装 vcpkg（官网源 / 国内镜像源）\n"
      << "      --china                   使用国内源（Gitee 仓库 + 清华下载镜像）\n"
      << "      --official                使用 GitHub 官方源\n"
      << "      -f, --force               强制重新安装\n"
      << "      --verbose                 输出安装的详细过程\n"
      << "      已安装时会提示位置并可选择跳过本次安装；非交互环境自动跳过\n"
      << "      示例: cppup vcpkg setup\n"
      << "            cppup vcpkg setup --china\n"
      << "            cppup vcpkg setup --official --force\n\n"

      << "  doctor                        检查 Clang / CMake / Ninja / vcpkg /\n"
      << "                                clang-format / clang-tidy 是否可用\n"
      << "      示例: cppup doctor\n\n"

      << "  templates                     列出可用的项目模板\n"
      << "  help                          显示本帮助\n"
      << "  version                       显示版本号\n\n"

      << util::bold("通用参数:\n")
      << "  -r, --release                 Release 构建（默认 Debug）\n"
      << "      --verbose                 输出详细过程\n"
      << "      --no-check                跳过 build / go 前的代码检查\n"
      << "      --fix                     对 check / build / go 应用自动修复\n"
      << "  -t, --template <名称>         init 使用的模板（默认 basic）\n"
      << "  -f, --force                   覆盖已有输出（init / bundle / vcpkg setup）\n"
      << "      --china                   使用国内 vcpkg 源\n"
      << "      --official                使用官方 vcpkg 源\n"
      << "  -v, --version                 显示版本号\n"
      << "  -h, --help                    显示本帮助\n\n"

      << util::bold("常用示例:\n")
      << "  cppup init Hello && cd Hello && cppup go\n"
      << "  cppup init MyLib -t modules\n"
      << "  cppup install fmt spdlog\n"
      << "  cppup check --fix\n"
      << "  cppup build --release --no-check\n"
      << "  cppup vcpkg setup --china\n"
      << "  cppup add ../shared/math.cppm\n"
      << "  cppup update\n"
      << "  cppup doctor\n"
      << "  # 首次构建 cppup 自身: ./bootstrap.sh\n"
      << "  # 之后: ./build/cppup build --release\n";
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