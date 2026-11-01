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

add_executable({{NAME}}
  src/main.cpp
)
target_compile_features({{NAME}} PRIVATE cxx_std_23)
set_target_properties({{NAME}} PROPERTIES CXX_EXTENSIONS OFF)

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
compile_commands.json
CMakeUserPresets.json
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

const char* BASIC_CLANGD_CONFIG = R"(CompileFlags:
  Add:
    - -std=c++23
  CompilationDatabase: build
)";

const char* MODULES_CLANGD_CONFIG = R"(CompileFlags:
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

A C++23 project created with cppup.

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

if(NOT CMAKE_CXX_COMPILER)
  if(DEFINED ENV{CXX})
    set(CMAKE_CXX_COMPILER "$ENV{CXX}" CACHE FILEPATH "C++ compiler" FORCE)
  else()
    find_program(CPPUP_CLANGXX NAMES clang++ clang++-23 clang++-22 clang++-21
      PATHS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
    if(CPPUP_CLANGXX)
      set(CMAKE_CXX_COMPILER "${CPPUP_CLANGXX}" CACHE FILEPATH "C++ compiler" FORCE)
    endif()
  endif()
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.2" AND NOT CMAKE_CXX_STDLIB_MODULES_JSON)
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -stdlib=libc++ -print-file-name=libc++.modules.json
    OUTPUT_VARIABLE CPPUP_LIBCXX_MODULES_JSON
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT EXISTS "${CPPUP_LIBCXX_MODULES_JSON}")
    get_filename_component(CPPUP_CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(CPPUP_LLVM_PREFIX "${CPPUP_CLANG_BIN_DIR}/.." ABSOLUTE)
    file(GLOB_RECURSE CPPUP_LIBCXX_MODULE_CANDIDATES LIST_DIRECTORIES FALSE
      "${CPPUP_LLVM_PREFIX}/lib/libc++.modules.json"
      "${CPPUP_LLVM_PREFIX}/lib/clang/*/libc++.modules.json"
      "${CPPUP_LLVM_PREFIX}/share/libc++/*/libc++.modules.json"
    )
    if(CPPUP_LIBCXX_MODULE_CANDIDATES)
      list(GET CPPUP_LIBCXX_MODULE_CANDIDATES 0 CPPUP_LIBCXX_MODULES_JSON)
    else()
      message(FATAL_ERROR "libc++.modules.json not found under ${CPPUP_LLVM_PREFIX}")
    endif()
  endif()
  set(CMAKE_CXX_STDLIB_MODULES_JSON "${CPPUP_LIBCXX_MODULES_JSON}" CACHE FILEPATH "libc++ modules metadata" FORCE)
endif()

set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -stdlib=libc++ -Wno-reserved-module-identifier")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -stdlib=libc++")

project({{NAME}} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_MODULE_STD ON)

add_executable({{NAME}} src/main.cpp)
target_compile_features({{NAME}} PRIVATE cxx_std_23)
include(modules/modules.cmake)
target_sources({{NAME}}
  PRIVATE
    FILE_SET CXX_MODULES FILES
    ${CPPUP_MODULE_SOURCES}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options({{NAME}} PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_custom_target(copy_module_bmis ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/modules"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/modules"
    COMMAND ${CMAKE_COMMAND}
      -DCPPUP_SOURCE_DIR=${CMAKE_SOURCE_DIR}
      -DCPPUP_BINARY_DIR=${CMAKE_BINARY_DIR}
      -DCPPUP_TARGET={{NAME}}
      -P "${CMAKE_SOURCE_DIR}/modules/copy-module-bmis.cmake"
    DEPENDS {{NAME}}
    VERBATIM
  )
endif()
)";

const char* MODULES_CMAKE_LIST = R"(set(CPPUP_MODULE_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/app.math.cppm"
  "${CMAKE_CURRENT_LIST_DIR}/app.service.cppm"
)
)";

const char* MODULES_COPY_BMIS = R"(set(_module_pcm_dirs
  "${CPPUP_BINARY_DIR}/CMakeFiles/${CPPUP_TARGET}.dir"
  "${CPPUP_BINARY_DIR}/CMakeFiles/@cmake_cxx_std.dir"
)

foreach(_pcm_dir IN LISTS _module_pcm_dirs)
  if(NOT IS_DIRECTORY "${_pcm_dir}")
    continue()
  endif()
  file(GLOB_RECURSE _module_pcms LIST_DIRECTORIES FALSE "${_pcm_dir}/*.pcm")
  foreach(_pcm IN LISTS _module_pcms)
    get_filename_component(_pcm_name "${_pcm}" NAME)
    file(COPY_FILE "${_pcm}" "${CPPUP_SOURCE_DIR}/modules/${_pcm_name}" ONLY_IF_DIFFERENT)
    file(COPY_FILE "${_pcm}" "${CPPUP_BINARY_DIR}/modules/${_pcm_name}" ONLY_IF_DIFFERENT)
  endforeach()
endforeach()
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
the C++ language mode. This template uses upstream Clang and libc++; use a recent
Clang/libc++ package that provides `libc++.modules.json`, `std.cppm`, and
`std.compat.cppm`. On macOS, install Homebrew LLVM rather than relying on
AppleClang's module metadata discovery.

## Modules and PCM files

Module interface sources live in `modules/*.cppm`. Add another local module with
`cppup add path/to/module.cppm`. With Clang, CMake copies generated PCM files
into `modules/` and `build/modules/` so clangd can load them. `cppup clean`
removes generated PCM files but preserves the module sources.

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
    files.push_back({".clangd", std::string(BASIC_CLANGD_CONFIG)});
  } else if (template_name == "modules") {
    files.push_back({"CMakeLists.txt", fill(MODULES_CMAKE, name)});
    files.push_back({"src/main.cpp", fill(MODULES_MAIN, name)});
    files.push_back({"modules/app.math.cppm", std::string(MODULES_MATH)});
    files.push_back({"modules/app.service.cppm", std::string(MODULES_SERVICE)});
    files.push_back({"modules/modules.cmake", std::string(MODULES_CMAKE_LIST)});
    files.push_back({"modules/copy-module-bmis.cmake", std::string(MODULES_COPY_BMIS)});
    files.push_back({".gitignore",     std::string(BASIC_GITIGNORE)});
    files.push_back({"README.md",      fill(MODULES_README, name)});
    files.push_back({".clangd", std::string(MODULES_CLANGD_CONFIG)});
  } else {
    return {}; // unknown
  }
  if (template_name == "modules") {
    files.push_back({"modules/.gitkeep", ""});
  }
  files.push_back({".vscode/launch.json", fill(LAUNCH_JSON, name)});
  return files;
}

} // namespace templates