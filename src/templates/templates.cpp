#include "templates/templates.hpp"
#include "util/util.hpp"

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
*.pcm

# IDE
.idea/
.vscode/*
!.vscode/launch.json
*.swp
*~
.DS_Store

# Compile commands (keep if you want)
# compile_commands.json
)";

const char* CLANGD_CONFIG = R"(CompileFlags:
  Add:
    - -std=c++23
    - -stdlib=libc++
    - -fprebuilt-module-path=modules
    - -Wno-reserved-module-identifier
  CompilationDatabase: build
)";

const char* LAUNCH_JSON = R"JSON({
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug (GDB)",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/{{NAME}}",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "externalConsole": false,
      "MIMode": "gdb",
      "setupCommands": [
        {
          "description": "Enable GDB pretty printing",
          "text": "-enable-pretty-printing",
          "ignoreFailures": true
        }
      ]
    },
    {
      "name": "Debug (LLDB)",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/{{NAME}}",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "externalConsole": false,
      "MIMode": "lldb"
    },
    {
      "name": "Debug (Windows)",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/{{NAME}}.exe",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "console": "integratedTerminal"
    }
  ]
})JSON";

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

// ---------- C++23 modules template ----------
const char* MODULES_CMAKE = R"(cmake_minimum_required(VERSION 3.30)

if(CMAKE_VERSION VERSION_LESS "4.0")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508" CACHE STRING "CMake import std gate" FORCE)
elseif(CMAKE_VERSION VERSION_LESS "4.1")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457" CACHE STRING "CMake import std gate" FORCE)
elseif(CMAKE_VERSION VERSION_LESS "4.3")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444" CACHE STRING "CMake import std gate" FORCE)
elseif(CMAKE_VERSION VERSION_LESS "4.4")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b" CACHE STRING "CMake import std gate" FORCE)
elseif(CMAKE_VERSION VERSION_LESS "4.5")
  set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d" CACHE STRING "CMake import std gate" FORCE)
else()
  message(FATAL_ERROR "Update the CMAKE_EXPERIMENTAL_CXX_IMPORT_STD gate for this CMake version")
endif()

project({{NAME}} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_MODULE_STD ON)

add_executable({{NAME}} src/main.cpp)
target_sources({{NAME}}
  PRIVATE
    FILE_SET CXX_MODULES FILES
    modules/app.math.cppm
    modules/app.service.cppm
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options({{NAME}} PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_custom_target(copy_module_bmis ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/modules"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/modules"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/CMakeFiles/{{NAME}}.dir/app.math.pcm"
      "${CMAKE_SOURCE_DIR}/modules/app.math.pcm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/CMakeFiles/{{NAME}}.dir/app.service.pcm"
      "${CMAKE_SOURCE_DIR}/modules/app.service.pcm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/CMakeFiles/@cmake_cxx_std.dir/std.pcm"
      "${CMAKE_SOURCE_DIR}/modules/std.pcm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/CMakeFiles/@cmake_cxx_std.dir/std.compat.pcm"
      "${CMAKE_SOURCE_DIR}/modules/std.compat.pcm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/modules/app.math.pcm"
      "${CMAKE_BINARY_DIR}/modules/app.math.pcm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/modules/app.service.pcm"
      "${CMAKE_BINARY_DIR}/modules/app.service.pcm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/modules/std.pcm"
      "${CMAKE_BINARY_DIR}/modules/std.pcm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/modules/std.compat.pcm"
      "${CMAKE_BINARY_DIR}/modules/std.compat.pcm"
    DEPENDS {{NAME}}
    VERBATIM
  )
endif()
)";

const char* MODULES_MAIN = R"(import std;
import app.math;
import app.service;

int main() {
  std::println("2 + 3 = {}", app::add(2, 3));
  std::println("C++23 modules are working");
  return 0;
}
)";

const char* MODULES_MATH = R"(export module app.math;

export namespace app {
int add(int lhs, int rhs) {
  return lhs + rhs;
}
}
)";

const char* MODULES_SERVICE = R"(export module app.service;

import app.math;

export namespace app {
int add_with_service(int lhs, int rhs) {
  return add(lhs, rhs);
}
}
)";

const char* MODULES_README = R"(# {{NAME}}

A C++23 modules project created with cppup.

## Requirements

- CMake 3.30 or newer
- Ninja 1.11 or newer
- A C++23 compiler and standard library with `import std;` support

`import std;` support depends on the compiler and its standard library, not just
the C++ language mode. Recent Clang with libc++, GCC 14 or newer with libstdc++,
or a recent Visual Studio toolset may work; verify your toolchain's module
support if configuration or compilation fails.

## Modules and PCM files

Module interface sources live in `modules/*.cppm`. With Clang, CMake copies
the generated `std` and application PCM files into `modules/` and `build/modules/`
so clangd can load them. `cppup clean` removes generated PCM files but preserves
the module sources.

## Build and run

```bash
cppup build
cppup run
```
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
    files.push_back({"modules/.gitkeep", ""});
    files.push_back({".gitignore",     std::string(BASIC_GITIGNORE)});
    files.push_back({"README.md",      fill(BASIC_README, name)});
  } else if (template_name == "modules") {
    files.push_back({"CMakeLists.txt", fill(MODULES_CMAKE, name)});
    files.push_back({"src/main.cpp", fill(MODULES_MAIN, name)});
    files.push_back({"modules/app.math.cppm", std::string(MODULES_MATH)});
    files.push_back({"modules/app.service.cppm", std::string(MODULES_SERVICE)});
    files.push_back({".gitignore",     std::string(BASIC_GITIGNORE)});
    files.push_back({"README.md",      fill(MODULES_README, name)});
  } else {
    return {}; // unknown
  }
  if (template_name == "modules") {
    files.push_back({"modules/.gitkeep", ""});
  }
  files.push_back({".clangd", std::string(CLANGD_CONFIG)});
  files.push_back({".vscode/launch.json", fill(LAUNCH_JSON, name)});
  return files;
}

} // namespace templates