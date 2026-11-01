#include "commands.hpp"
#include "util.hpp"

int main(int argc, char** argv) {
  util::set_color(true);
  auto args = commands::parse(argc, argv);
  return commands::dispatch(args);
}