#include "cli/commands.hpp"
#include "util/util.hpp"

int main(int argc, char** argv) {
  util::set_color(true);
  auto args = commands::parse(argc, argv);
  return commands::dispatch(args);
}