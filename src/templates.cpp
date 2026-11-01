#include "templates.hpp"
#include "util.hpp"

namespace templates {

namespace {

// ---------- basic C++23 template ----------
const char* BASIC_CMAKE = R"(cmake_minimum_required(VERSION 3.20)
project({{NAME}} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Ninja
if(NOT CMAKE_GENERATOR)
  find_program(NINJA_EXECUTABLE ninja)
  if(NINJA_EXECUTABLE)
    set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)
  endif()
endif()

add_executable({{NAME}}
  src/main.cpp
)

# Optional: stricter warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options({{NAME}} PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
  )
endif()

# Install
install(TARGETS {{NAME}} RUNTIME DESTINATION bin)
)";

const char* BASIC_MAIN = R"(#include <iostream>
#include <string>

// C++23-ready starter. Uses classic iostream for maximum compiler compatibility.
// Once your toolchain has full <print> support you can switch to std::println.

int main() {
  std::cout << "Hello from {{NAME}}!\n";
  std::cout << "C++23 project created by cppup.\n";
  return 0;
}
)";

const char* BASIC_GITIGNORE = R"(# Build
build/
cmake-build-*/
out/
*.o
*.a
*.so
*.dylib
*.exe
*.dll
*.pdb

# IDE
.idea/
.vscode/
*.swp
*~
.DS_Store

# Compile commands (keep if you want)
# compile_commands.json
)";

const char* BASIC_README = R"(# {{NAME}}

A C++23 project created with [cppup](https://github.com/example/cppup).

## Build

```bash
cppup build
# or manually:
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Run

```bash
cppup run
# or
./build/{{NAME}}
```

## One-shot

```bash
cppup go
```
)";

// ---------- modules template (placeholder for later) ----------
const char* MODULES_CMAKE = R"(cmake_minimum_required(VERSION 3.28)
project({{NAME}} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_executable({{NAME}})
target_sources({{NAME}}
  PRIVATE
    FILE_SET CXX_MODULES FILES
      src/main.cppm
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options({{NAME}} PRIVATE -Wall -Wextra -Wpedantic)
endif()
)";

const char* MODULES_MAIN = R"(export module main;

import std;

export int main() {
  std::println("Hello from {{NAME}} (C++ Modules)!");
  return 0;
}
)";

std::string fill(std::string_view tmpl, std::string_view name) {
  return util::replace_all(std::string(tmpl), "{{NAME}}", name);
}

} // namespace

std::vector<std::string> list() {
  return {"basic", "modules"};
}

std::vector<FileEntry> generate(std::string_view template_name,
                                std::string_view project_name) {
  std::vector<FileEntry> files;
  std::string name(project_name);

  if (template_name == "basic" || template_name.empty()) {
    files.push_back({"CMakeLists.txt", fill(BASIC_CMAKE, name)});
    files.push_back({"src/main.cpp",   fill(BASIC_MAIN, name)});
    files.push_back({".gitignore",     std::string(BASIC_GITIGNORE)});
    files.push_back({"README.md",      fill(BASIC_README, name)});
  } else if (template_name == "modules") {
    files.push_back({"CMakeLists.txt", fill(MODULES_CMAKE, name)});
    files.push_back({"src/main.cppm",  fill(MODULES_MAIN, name)});
    files.push_back({".gitignore",     std::string(BASIC_GITIGNORE)});
    files.push_back({"README.md",      fill(BASIC_README, name)});
  } else {
    return {}; // unknown
  }
  return files;
}

} // namespace templates