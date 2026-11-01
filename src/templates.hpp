#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace templates {

struct FileEntry {
  std::string relative_path;   // e.g. "src/main.cpp"
  std::string content;         // already with {{NAME}} etc replaced or raw
};

// Available template names
std::vector<std::string> list();

// Generate file list for a template. name is the project name.
// Returns empty on unknown template.
std::vector<FileEntry> generate(std::string_view template_name,
                                std::string_view project_name);

} // namespace templates