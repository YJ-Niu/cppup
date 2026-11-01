#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace commands {

struct Args {
  std::string command;
  std::vector<std::string> positional;
  // common flags
  bool help = false;
  bool force = false;
  bool release = false;
  bool verbose = false;
  std::string template_name = "basic";
};

Args parse(int argc, char** argv);

void print_help(std::string_view program = "cppup");
void print_version();

int dispatch(const Args& args);

} // namespace commands