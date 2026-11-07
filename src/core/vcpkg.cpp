#include "core/vcpkg.hpp"
#include "util/util.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <regex>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace vcpkg {

namespace {

#ifdef _WIN32
#define CPPUP_ISATTY _isatty
#define CPPUP_FILENO _fileno
#else
#define CPPUP_ISATTY isatty
#define CPPUP_FILENO fileno
#endif

const char* OFFICIAL_REPOSITORY = "https://github.com/microsoft/vcpkg.git";
const char* CHINA_REPOSITORY = "https://gitee.com/mirrors/vcpkg.git";
const char* CHINA_ASSET_MIRROR =
    "https://mirrors.tuna.tsinghua.edu.cn/vcpkg/assets/";

std::string env_value(const char* name) {
  if (const char* value = std::getenv(name)) return value;
  return {};
}

fs::path home_directory() {
#ifdef _WIN32
  if (auto profile = env_value("USERPROFILE"); !profile.empty()) {
    return fs::path(profile);
  }
#endif
  return fs::path(env_value("HOME"));
}

// Default install directory chosen per operating system.
fs::path default_root() {
#if defined(_WIN32)
  if (auto local = env_value("LOCALAPPDATA"); !local.empty()) {
    return fs::path(local) / "cppup" / "vcpkg";
  }
  if (auto profile = env_value("USERPROFILE"); !profile.empty()) {
    return fs::path(profile) / "AppData" / "Local" / "cppup" / "vcpkg";
  }
  return home_directory() / ".cppup" / "vcpkg";
#elif defined(__APPLE__)
  return home_directory() / "Library" / "Application Support" / "cppup" /
         "vcpkg";
#else
  if (auto data = env_value("XDG_DATA_HOME"); !data.empty()) {
    return fs::path(data) / "cppup" / "vcpkg";
  }
  return home_directory() / ".local" / "share" / "cppup" / "vcpkg";
#endif
}

fs::path config_path() {
  return home_directory() / ".cppup" / "config.json";
}

bool is_regular(const fs::path& path) {
  std::error_code ec;
  return fs::is_regular_file(path, ec);
}

bool is_executable_file(const fs::path& path) {
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) return false;
#ifdef _WIN32
  return true;
#else
  auto perms = fs::status(path, ec).permissions();
  using fs::perms;
  return (perms & (perms::owner_exec | perms::group_exec | perms::others_exec)) !=
         perms::none;
#endif
}

fs::path executable_in(const fs::path& root) {
#ifdef _WIN32
  return root / "vcpkg.exe";
#else
  return root / "vcpkg";
#endif
}

bool has_root_marker(const fs::path& root) {
  return is_regular(root / ".vcpkg-root");
}

fs::path first_line_path(const std::string& text) {
  auto line = util::trim(text);
  const auto newline = line.find('\n');
  if (newline != std::string::npos) line = line.substr(0, newline);
  return fs::path(util::trim(line));
}

fs::path vcpkg_on_path() {
#ifdef _WIN32
  auto result = util::run({"where", "vcpkg"}, {}, true, true);
#else
  auto result = util::run({"which", "vcpkg"}, {}, true, true);
#endif
  if (result.exit_code != 0) return {};
  auto path = first_line_path(result.stdout_str);
  return is_executable_file(path) ? path : fs::path{};
}

Location probe_root(const fs::path& root) {
  Location location;
  if (root.empty()) return location;
  location.root = root;
  location.executable = executable_in(root);
  if (!is_regular(root / "scripts" / "buildsystems" / "vcpkg.cmake")) {
    return location;
  }
  if (!is_executable_file(location.executable)) {
    return location;
  }
  location.found = true;
  return location;
}

struct GlobalConfig {
  Source source = Source::Official;
  bool has_source = false;
  std::string root;
  std::string triplet;
};

GlobalConfig read_global_config() {
  GlobalConfig config;
  auto text = util::read_file(config_path());
  if (!text) return config;

  static const std::regex source_pattern(R"rx("vcpkgSource"\s*:\s*"([^"]*)")rx");
  static const std::regex root_pattern(R"rx("vcpkgRoot"\s*:\s*"([^"]*)")rx");
  static const std::regex triplet_pattern(R"rx("vcpkgTriplet"\s*:\s*"([^"]*)")rx");
  std::smatch match;
  if (std::regex_search(*text, match, source_pattern)) {
    if (auto source = parse_source(match[1].str())) {
      config.source = *source;
      config.has_source = true;
    }
  }
  if (std::regex_search(*text, match, root_pattern)) config.root = match[1].str();
  if (std::regex_search(*text, match, triplet_pattern)) {
    config.triplet = match[1].str();
  }
  return config;
}

std::string json_escape(std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size() + 8);
  for (char c : text) {
    switch (c) {
      case '\\': escaped += "\\\\"; break;
      case '"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default: escaped += c;
    }
  }
  return escaped;
}

std::string now_iso() {
  std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char buffer[32] = {};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &local);
  return buffer;
}

void write_global_config(Source source, const fs::path& root) {
  if (root.empty()) return;
  const std::string text =
      "{\n"
      "  \"vcpkgRoot\": \"" + json_escape(root.generic_string()) + "\",\n"
      "  \"vcpkgSource\": \"" + source_name(source) + "\",\n"
      "  \"vcpkgTriplet\": \"" + host_triplet() + "\"\n"
      "}\n";
  util::write_file(config_path(), text);
}

bool interactive_stdin() {
  return CPPUP_ISATTY(CPPUP_FILENO(stdin)) != 0;
}

std::string sanitize_port_name(std::string name) {
  std::string clean;
  for (char c : name) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (std::isalnum(u) || c == '-' || c == '_') clean += c;
  }
  if (clean.empty()) return clean;
  if (!(std::isalpha(static_cast<unsigned char>(clean[0]))) && clean[0] != '_') {
    clean.insert(clean.begin(), 'p');
  }
  return clean;
}

std::string sanitize_manifest_name(std::string name) {
  std::string clean;
  for (char c : name) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (std::isalnum(u)) clean += static_cast<char>(std::tolower(u));
    else if (c == '_' || c == '-' || c == '.') clean += '-';
  }
  while (!clean.empty() && clean.front() == '-') clean.erase(clean.begin());
  while (!clean.empty() && clean.back() == '-') clean.pop_back();
  if (clean.empty()) clean = "cppup-project";
  if (std::isdigit(static_cast<unsigned char>(clean.front()))) {
    clean.insert(0, "cppup-");
  }
  return clean;
}

std::string git_output(const fs::path& root, const std::vector<std::string>& args) {
  std::vector<std::string> command = {"git", "-C", root.string()};
  command.insert(command.end(), args.begin(), args.end());
  auto result = util::run(command, {}, true, true);
  if (result.exit_code != 0) return {};
  return util::trim(result.stdout_str);
}

bool write_manifest(const fs::path& project_root, std::string_view name) {
  const std::string manifest =
      "{\n"
      "  \"name\": \"" + json_escape(sanitize_manifest_name(std::string(name))) + "\",\n"
      "  \"version-string\": \"0.1.0\",\n"
      "  \"description\": \"C++ project created by cppup\",\n"
      "  \"dependencies\": []\n"
      "}\n";
  return util::write_file(project_root / "vcpkg.json", manifest);
}

bool write_configuration(const fs::path& project_root, Source source) {
  if (source != Source::ChinaMirror) return true;

  const auto location = locate(project_root);
  if (location.root.empty()) return true;

  const auto remote = git_output(location.root, {"remote", "get-url", "origin"});
  if (remote.find("gitee") == std::string::npos) {
    // vcpkg itself comes from GitHub; keep the builtin registry and only
    // redirect package downloads to the domestic asset mirror.
    return true;
  }

  const auto baseline = git_output(location.root, {"rev-parse", "HEAD"});
  if (baseline.empty()) return true;

  const std::string text =
      "{\n"
      "  \"default-registry\": {\n"
      "    \"kind\": \"git\",\n"
      "    \"repository\": \"" + std::string(CHINA_REPOSITORY) + "\",\n"
      "    \"baseline\": \"" + baseline + "\"\n"
      "  }\n"
      "}\n";
  return util::write_file(project_root / "vcpkg-configuration.json", text);
}

void report_source(Source source) {
  const Mirror selected = mirror(source);
  util::info("vcpkg source: " + source_name(source));
  std::cout << util::dim("  repository: " + selected.repository) << "\n";
  if (!selected.asset_sources.empty()) {
    std::cout << util::dim("  assets:     " + selected.asset_sources) << "\n";
  }
  std::cout << util::dim("  switch source with 'cppup vcpkg setup --official' "
                         "or '--china'")
            << "\n";
}

} // namespace

std::string source_name(Source source) {
  return source == Source::ChinaMirror ? "china" : "official";
}

std::optional<Source> parse_source(std::string_view text) {
  std::string value;
  value.reserve(text.size());
  for (char c : text) {
    value += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (value == "official" || value == "github" || value == "1") {
    return Source::Official;
  }
  if (value == "china" || value == "cn" || value == "mirror" ||
      value == "gitee" || value == "2") {
    return Source::ChinaMirror;
  }
  return std::nullopt;
}

Mirror mirror(Source source) {
  if (source == Source::ChinaMirror) {
    Mirror result;
    result.repository = env_value("CPPUP_VCPKG_GIT_MIRROR");
    if (result.repository.empty()) result.repository = CHINA_REPOSITORY;
    result.asset_sources = env_value("CPPUP_VCPKG_ASSET_MIRROR");
    if (result.asset_sources.empty()) result.asset_sources = CHINA_ASSET_MIRROR;
    return result;
  }
  return {OFFICIAL_REPOSITORY, {}};
}

Source configured_source() {
  const auto from_env = env_value("CPPUP_VCPKG_SOURCE");
  if (auto parsed = parse_source(from_env)) return *parsed;
  const auto config = read_global_config();
  if (config.has_source) return config.source;
  return Source::Official;
}

Source prompt_source() {
  if (auto parsed = parse_source(env_value("CPPUP_VCPKG_SOURCE"))) return *parsed;
  if (auto parsed = parse_source(env_value("CPPUP_VCPKG_MIRROR"))) return *parsed;
  if (!interactive_stdin()) return configured_source();

  const Source current = configured_source();
  const char* default_choice = current == Source::ChinaMirror ? "2" : "1";

  std::cout << util::bold("Select vcpkg source / 选择 vcpkg 安装源") << "\n"
            << "  Current / 当前配置: " << util::cyan(source_name(current)) << "\n"
            << "  Install to / 安装目录: " << util::cyan(configured_root().string())
            << "\n"
            << "  1) Official GitHub (官网源，默认)\n"
            << "     clone microsoft/vcpkg from GitHub\n"
            << "     needs stable access to GitHub\n"
            << "  2) China mirror (国内源，中国大陆推荐)\n"
            << "     clone the Gitee mirror, download assets from the "
               "Tsinghua mirror\n"
            << "     falls back to the official source when unavailable\n"
            << "Choice / 选择 [1/2] (default " << default_choice << "): ";
  std::cout.flush();
  std::string line;
  if (!std::getline(std::cin, line)) line.clear();
  line = util::trim(line);
  if (line.empty()) return current;
  if (auto parsed = parse_source(line)) return *parsed;
  util::warn("unknown choice '" + line + "', using official source");
  return Source::Official;
}

SetupChoice prompt_setup(const Location& existing) {
  SetupChoice choice;
  choice.source = configured_source();

  if (existing.found) {
    util::success("vcpkg already installed at " + existing.root.string());
    if (!interactive_stdin()) {
      choice.skip = true;
      return choice;
    }
    std::cout << util::bold("vcpkg is already installed / vcpkg 已安装") << "\n"
              << "  1) Skip this installation (use the installed vcpkg) / "
                 "跳过本次安装\n"
              << "  2) Use the official source (update config) / 官网源\n"
              << "  3) Use the China mirror (update config) / 国内源\n"
              << "Choice / 选择 [1/2/3] (default 1): ";
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) line.clear();
    line = util::trim(line);
    if (line.empty() || line == "1" || line == "skip") {
      choice.skip = true;
      return choice;
    }
    if (line == "2") {
      choice.source = Source::Official;
      return choice;
    }
    if (line == "3") {
      choice.source = Source::ChinaMirror;
      return choice;
    }
    if (auto parsed = parse_source(line)) {
      choice.source = *parsed;
      return choice;
    }
    util::warn("unknown choice '" + line + "', skipping installation");
    choice.skip = true;
    return choice;
  }

  util::info("vcpkg is not installed; it will be installed in " +
             configured_root().string());
  choice.source = prompt_source();
  return choice;
}

fs::path configured_root() {
  if (auto root = env_value("VCPKG_ROOT"); !root.empty()) return fs::path(root);
  const auto config = read_global_config();
  if (!config.root.empty()) return fs::path(config.root);
  return default_root();
}

Location locate(const fs::path& project_root) {
  std::vector<fs::path> roots;
  if (auto root = env_value("VCPKG_ROOT"); !root.empty()) {
    roots.emplace_back(root);
  }
  const auto config = read_global_config();
  if (!config.root.empty()) roots.emplace_back(config.root);
  roots.push_back(default_root());
  roots.push_back(home_directory() / ".cppup" / "vcpkg");
  roots.push_back(home_directory() / "vcpkg");
  if (!project_root.empty()) {
    roots.push_back(project_root / ".vcpkg");
    roots.push_back(project_root / "vcpkg");
  }

  for (const auto& root : roots) {
    auto location = probe_root(root);
    if (location.found) return location;
  }

  const auto on_path = vcpkg_on_path();
  if (!on_path.empty()) {
    auto location = probe_root(on_path.parent_path());
    if (location.found) return location;
    Location fallback;
    fallback.root = on_path.parent_path();
    fallback.executable = on_path;
    fallback.found = true;
    if (auto version = util::command_version(on_path.string())) {
      fallback.version = *version;
    }
    return fallback;
  }
  return {};
}

std::string host_triplet() {
  if (auto triplet = env_value("VCPKG_DEFAULT_TRIPLET"); !triplet.empty()) {
    return triplet;
  }
#if defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
  return "arm64-osx";
#else
  return "x64-osx";
#endif
#elif defined(_WIN32)
  return "x64-windows";
#else
#if defined(__aarch64__) || defined(__arm64__)
  return "arm64-linux";
#else
  return "x64-linux";
#endif
#endif
}

bool setup(Source source, bool force, bool verbose) {
  const fs::path root = configured_root();
  const auto existing = probe_root(root);
  if (existing.found && !force) {
    write_global_config(source, root);
    util::success("vcpkg already installed at " + root.string());
    report_source(source);
    return true;
  }

  if (!util::command_exists("git")) {
    util::error("git is required to install vcpkg");
    return false;
  }

  if (fs::exists(root) && !has_root_marker(root) && !existing.found) {
    if (!force) {
      util::error("incomplete vcpkg directory at " + root.string() +
                  "\n  rerun with --force to replace it");
      return false;
    }
    std::error_code ec;
    fs::remove_all(root, ec);
    if (ec) {
      util::error("cannot remove " + root.string() + ": " + ec.message());
      return false;
    }
  }

  Source actual = source;
  if (!has_root_marker(root)) {
    std::error_code ec;
    fs::create_directories(root.parent_path(), ec);
    auto repository = mirror(actual).repository;
    util::info("cloning vcpkg (" + source_name(actual) + ") from " + repository);
    auto clone = util::run({"git", "clone", "--depth", "1", repository,
                            root.string()},
                           {}, false, !verbose);
    if (clone.exit_code != 0 && actual == Source::ChinaMirror) {
      util::warn("China mirror clone failed; retrying the official repository");
      actual = Source::Official;
      clone = util::run({"git", "clone", "--depth", "1",
                         mirror(actual).repository, root.string()},
                        {}, false, !verbose);
    }
    if (clone.exit_code != 0) {
      util::error("failed to clone vcpkg.\n"
                  "  Check your network, or set HTTPS_PROXY, then retry\n"
                  "  'cppup vcpkg setup --official'");
      return false;
    }
  }

  const fs::path bootstrap =
#ifdef _WIN32
      root / "bootstrap-vcpkg.bat";
#else
      root / "bootstrap-vcpkg.sh";
#endif
  if (!is_regular(bootstrap)) {
    util::error("vcpkg bootstrap script not found: " + bootstrap.string());
    return false;
  }

  std::vector<std::string> command;
#ifdef _WIN32
  command = {bootstrap.string(), "-disableMetrics"};
#else
  command = {"sh", bootstrap.string(), "-disableMetrics"};
#endif
  auto boot = util::run(command, root, false, !verbose);
  if (boot.exit_code != 0) {
    util::error("vcpkg bootstrap failed.\n"
                "  If GitHub is unreachable, set HTTPS_PROXY and retry, or use\n"
                "  'cppup vcpkg setup --official' after the download works.");
    return false;
  }

  const auto installed = probe_root(root);
  if (!installed.found) {
    util::error("vcpkg bootstrap finished but the binary was not found at " +
                executable_in(root).string());
    return false;
  }

  write_global_config(actual, root);
  util::success("vcpkg installed at " + root.string());
  report_source(actual);
  std::cout << util::dim("  Add this to your shell profile:")
            << "\n  export VCPKG_ROOT=\"" << root.string() << "\"\n";
  return true;
}

bool has_manifest(const fs::path& project_root) {
  return is_regular(project_root / "vcpkg.json");
}

bool ensure_manifest(const fs::path& project_root, std::string_view project_name,
                     Source source) {
  bool ok = true;
  if (!has_manifest(project_root)) {
    ok = write_manifest(project_root, project_name);
    if (!ok) {
      util::error("could not write vcpkg.json");
      return false;
    }
  }
  if (!write_configuration(project_root, source)) {
    util::error("could not write vcpkg-configuration.json");
    return false;
  }
  return ok;
}

fs::path installed_dir(const fs::path& project_root) {
  return project_root / "build" / "vcpkg_installed" / host_triplet();
}

fs::path include_dir(const fs::path& project_root) {
  return installed_dir(project_root) / "include";
}

fs::path toolchain_file(const fs::path& project_root) {
  const auto location = locate(project_root);
  if (!location.found) return {};
  const fs::path toolchain =
      location.root / "scripts" / "buildsystems" / "vcpkg.cmake";
  return is_regular(toolchain) ? toolchain : fs::path{};
}

std::vector<std::pair<std::string, std::string>> install_env(
    const fs::path& project_root) {
  const auto source = read_project_config(project_root);
  Source selected = source ? source->source : configured_source();
  if (selected != Source::ChinaMirror) return {};
  auto config = mirror(selected);
  if (config.asset_sources.empty()) return {};
  return {{"X_VCPKG_ASSET_SOURCES",
           "x-azurl," + config.asset_sources + ",read"}};
}

bool install(const fs::path& project_root, const std::vector<std::string>& packages) {
  auto location = locate(project_root);
  if (!location.found) {
    util::error("vcpkg is not installed.\n"
                "  Run 'cppup vcpkg setup' first (it lets you choose the "
                "official or China mirror).");
    return false;
  }

  ProjectConfig config;
  bool had_config = false;
  if (auto existing = read_project_config(project_root)) {
    config = *existing;
    had_config = true;
  }
  if (!had_config) config.source = configured_source();
  if (config.name.empty()) config.name = project_root.filename().string();

  if (!ensure_manifest(project_root, config.name, config.source)) return false;

  const auto env = install_env(project_root);
  for (const auto& package : packages) {
    const auto name = sanitize_port_name(package);
    if (name.empty() || name != package) {
      util::error("invalid vcpkg port name: " + package);
      return false;
    }
    auto add = util::run({location.executable.string(), "add", "port", name},
                         project_root, true, true, env);
    if (add.exit_code != 0) {
      std::string output = add.stdout_str;
      std::transform(output.begin(), output.end(), output.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (output.find("already") == std::string::npos) {
        util::error("vcpkg could not add port '" + name + "'");
        std::cout << util::dim(add.stdout_str) << "\n";
        return false;
      }
    }
  }

  std::vector<std::string> command = {
      location.executable.string(), "install", "--triplet", host_triplet(),
      "--x-install-root=" + installed_dir(project_root).string(),
  };
  util::info("running vcpkg install (triplet " + host_triplet() + ")");
  auto result = util::run(command, project_root, false, false, env);
  if (result.exit_code != 0) {
    util::error("vcpkg install failed");
    return false;
  }

  for (const auto& package : packages) {
    const auto name = sanitize_port_name(package);
    if (std::find(config.packages.begin(), config.packages.end(), name) ==
        config.packages.end()) {
      config.packages.push_back(name);
    }
  }
  write_project_config(project_root, config);

  util::success("Packages installed into " + installed_dir(project_root).string());
  util::info("clangd include path: " + include_dir(project_root).string());
  return true;
}

std::optional<ProjectConfig> read_project_config(const fs::path& project_root) {
  auto text = util::read_file(project_root / ".cppup.json");
  if (!text) return std::nullopt;

  ProjectConfig config;
  static const std::regex name_pattern(R"rx("projectName"\s*:\s*"([^"]*)")rx");
  static const std::regex template_pattern(R"rx("template"\s*:\s*"([^"]*)")rx");
  static const std::regex source_pattern(R"rx("source"\s*:\s*"([^"]*)")rx");
  static const std::regex packages_pattern(R"rx("packages"\s*:\s*\[([^\]]*)\])rx");
  std::smatch match;
  if (std::regex_search(*text, match, name_pattern)) config.name = match[1].str();
  if (std::regex_search(*text, match, template_pattern)) {
    config.template_name = match[1].str();
  }
  if (std::regex_search(*text, match, source_pattern)) {
    if (auto source = parse_source(match[1].str())) config.source = *source;
  }
  if (std::regex_search(*text, match, packages_pattern)) {
    const std::string list = match[1].str();
    static const std::regex item_pattern(R"rx("([^"]*)")rx");
    for (std::sregex_iterator it(list.begin(), list.end(), item_pattern), end;
         it != end; ++it) {
      config.packages.push_back((*it)[1].str());
    }
  }
  return config;
}

bool write_project_config(const fs::path& project_root,
                          const ProjectConfig& config) {
  const auto location = locate(project_root);
  const std::string root = location.found ? location.root.generic_string()
                                          : configured_root().generic_string();

  std::string packages;
  for (size_t i = 0; i < config.packages.size(); ++i) {
    if (i) packages += ",\n";
    packages += "    \"" + json_escape(config.packages[i]) + "\"";
  }

  const std::string text =
      "{\n"
      "  \"schemaVersion\": 1,\n"
      "  \"projectName\": \"" + json_escape(config.name) + "\",\n"
      "  \"template\": \"" + json_escape(config.template_name) + "\",\n"
      "  \"vcpkg\": {\n"
      "    \"source\": \"" + source_name(config.source) + "\",\n"
      "    \"triplet\": \"" + host_triplet() + "\",\n"
      "    \"root\": \"" + json_escape(root) + "\",\n"
      "    \"installedDir\": \"" +
      json_escape(installed_dir(project_root).lexically_relative(project_root)
                      .generic_string()) +
      "\",\n"
      "    \"packages\": [" + (packages.empty() ? "]" : "\n" + packages + "\n    ]") +
      "\n"
      "  },\n"
      "  \"updatedAt\": \"" + now_iso() + "\"\n"
      "}\n";

  if (!util::write_file(project_root / ".cppup.json", text)) {
    util::error("could not write .cppup.json");
    return false;
  }
  return true;
}

} // namespace vcpkg
