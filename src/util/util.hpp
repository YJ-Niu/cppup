#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace util {

// Color output (simple ANSI, disabled on Windows or if NO_COLOR)
bool supports_color();
void set_color(bool enable);

std::string bold(std::string_view s);
std::string green(std::string_view s);
std::string red(std::string_view s);
std::string yellow(std::string_view s);
std::string cyan(std::string_view s);
std::string dim(std::string_view s);

// Run external command, return exit code. Capture stdout/stderr optionally.
struct CmdResult {
  int exit_code = -1;
  std::string stdout_str;
  std::string stderr_str;
};

CmdResult run(const std::vector<std::string>& args,
              const fs::path& cwd = {},
              bool capture = false,
              bool quiet = false);

// Check if executable exists in PATH
bool command_exists(std::string_view name);

// Get version string of a command (best-effort)
std::optional<std::string> command_version(std::string_view name,
                                           std::string_view version_flag = "--version");

// Write text file, create parent dirs if needed
bool write_file(const fs::path& path, std::string_view content);

// Read entire file
std::optional<std::string> read_file(const fs::path& path);

// Simple string replace all
std::string replace_all(std::string str, std::string_view from, std::string_view to);

// Join paths / strings
std::string join(const std::vector<std::string>& parts, std::string_view sep);

// Trim
std::string trim(std::string_view s);

// Print error and exit
[[noreturn]] void fatal(std::string_view msg);

void info(std::string_view msg);
void success(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);

} // namespace util