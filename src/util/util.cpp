#include "util/util.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace util {

static bool g_color = true;

bool supports_color() {
#ifdef _WIN32
  return false; // simplify for now
#else
  if (std::getenv("NO_COLOR")) return false;
  if (!isatty(fileno(stdout))) return false;
  return true;
#endif
}

void set_color(bool enable) { g_color = enable && supports_color(); }

std::string bold(std::string_view s) {
  if (!g_color) return std::string(s);
  return "\033[1m" + std::string(s) + "\033[0m";
}
std::string green(std::string_view s) {
  if (!g_color) return std::string(s);
  return "\033[32m" + std::string(s) + "\033[0m";
}
std::string red(std::string_view s) {
  if (!g_color) return std::string(s);
  return "\033[31m" + std::string(s) + "\033[0m";
}
std::string yellow(std::string_view s) {
  if (!g_color) return std::string(s);
  return "\033[33m" + std::string(s) + "\033[0m";
}
std::string cyan(std::string_view s) {
  if (!g_color) return std::string(s);
  return "\033[36m" + std::string(s) + "\033[0m";
}
std::string dim(std::string_view s) {
  if (!g_color) return std::string(s);
  return "\033[2m" + std::string(s) + "\033[0m";
}

CmdResult run(const std::vector<std::string>& args, const fs::path& cwd,
              bool capture, bool quiet) {
  if (args.empty()) return {-1, "", "empty command"};

  std::string cmd;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i) cmd += ' ';
    // simple quoting
    if (args[i].find(' ') != std::string::npos ||
        args[i].find('"') != std::string::npos) {
      cmd += '"';
      for (char c : args[i]) {
        if (c == '"') cmd += "\\\"";
        else cmd += c;
      }
      cmd += '"';
    } else {
      cmd += args[i];
    }
  }

  if (!cwd.empty()) {
#ifdef _WIN32
    cmd = "cd /d \"" + cwd.string() + "\" && " + cmd;
#else
    cmd = "cd \"" + cwd.string() + "\" && " + cmd;
#endif
  }

  if (!quiet && !capture) {
    std::cout << dim("$ " + cmd) << "\n";
  }

  CmdResult res;
  if (capture) {
    std::array<char, 4096> buffer;
    std::string out;
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) {
      res.exit_code = -1;
      res.stderr_str = "popen failed";
      return res;
    }
    while (fgets(buffer.data(), buffer.size(), pipe)) {
      out += buffer.data();
    }
    res.exit_code = pclose(pipe);
#ifdef _WIN32
    // Windows returns the exit code directly
#else
    if (WIFEXITED(res.exit_code))
      res.exit_code = WEXITSTATUS(res.exit_code);
#endif
    res.stdout_str = out;
  } else {
    res.exit_code = std::system(cmd.c_str());
#ifdef _WIN32
#else
    if (WIFEXITED(res.exit_code))
      res.exit_code = WEXITSTATUS(res.exit_code);
#endif
  }
  return res;
}

bool command_exists(std::string_view name) {
#ifdef _WIN32
  auto r = run({"where", std::string(name)}, {}, true, true);
#else
  auto r = run({"which", std::string(name)}, {}, true, true);
#endif
  return r.exit_code == 0 && !r.stdout_str.empty();
}

std::optional<std::string> command_version(std::string_view name,
                                           std::string_view version_flag) {
  auto r = run({std::string(name), std::string(version_flag)}, {}, true, true);
  if (r.exit_code != 0) return std::nullopt;
  // take first non-empty line
  std::istringstream iss(r.stdout_str);
  std::string line;
  while (std::getline(iss, line)) {
    line = trim(line);
    if (!line.empty()) return line;
  }
  return std::nullopt;
}

bool write_file(const fs::path& path, std::string_view content) {
  try {
    if (path.has_parent_path()) {
      fs::create_directories(path.parent_path());
    }
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return ofs.good();
  } catch (...) {
    return false;
  }
}

std::optional<std::string> read_file(const fs::path& path) {
  try {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return std::nullopt;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
  } catch (...) {
    return std::nullopt;
  }
}

std::string replace_all(std::string str, std::string_view from,
                        std::string_view to) {
  if (from.empty()) return str;
  size_t pos = 0;
  while ((pos = str.find(from, pos)) != std::string::npos) {
    str.replace(pos, from.size(), to);
    pos += to.size();
  }
  return str;
}

std::string join(const std::vector<std::string>& parts, std::string_view sep) {
  std::string r;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) r += sep;
    r += parts[i];
  }
  return r;
}

std::string trim(std::string_view s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return std::string(s.substr(b, e - b));
}

void fatal(std::string_view msg) {
  std::cerr << red("error: ") << msg << "\n";
  std::exit(1);
}

void info(std::string_view msg) {
  std::cout << cyan("info: ") << msg << "\n";
}

void success(std::string_view msg) {
  std::cout << green("✓ ") << msg << "\n";
}

void warn(std::string_view msg) {
  std::cout << yellow("warning: ") << msg << "\n";
}

void error(std::string_view msg) {
  std::cerr << red("error: ") << msg << "\n";
}

} // namespace util