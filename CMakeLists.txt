cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h" CPPUP_VERSION_LINE
  REGEX "^#define CPPUP_VERSION ")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" CPPUP_VERSION "${CPPUP_VERSION_LINE}")
if(NOT CPPUP_VERSION)
  message(FATAL_ERROR "Could not read CPPUP_VERSION from src/version.h")
endif()

option(CPPUP_ENABLE_CXX_MODULES
  "Enable C++23 standard library module support for cppup targets"
  OFF
)

if(CPPUP_ENABLE_CXX_MODULES)
  if(CMAKE_VERSION VERSION_LESS "3.30")
    message(FATAL_ERROR "CPPUP_ENABLE_CXX_MODULES requires CMake 3.30 or newer")
  elseif(CMAKE_VERSION VERSION_LESS "4.0")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508" CACHE STRING "CMake import std gate" FORCE)
  elseif(CMAKE_VERSION VERSION_LESS "4.1")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457" CACHE STRING 'CMake import std gate' FORCE)
  elseif(CMAKE_VERSION VERSION_LESS "4.3")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444" CACHE STRING 'CMake import std gate' FORCE)
  elseif(CMAKE_VERSION VERSION_LESS "4.4")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b" CACHE STRING "CMake import std gate" FORCE)
  elseif(CMAKE_VERSION VERSION_LESS "4.5")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d" CACHE STRING "CMake import std gate" FORCE)
  else()
    message(FATAL_ERROR "Update the CMAKE_EXPERIMENTAL_CXX_IMPORT_STD gate for this CMake version")
  endif()

  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
  set(CMAKE_CXX_MODULE_STD ON)
  set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -stdlib=libc++ -Wno-reserved-module-identifier")
  set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -stdlib=libc++")
endif()

# Target CPU architecture on macOS (e.g. x86_64 for Intel builds).
# Example: cmake -B build -DCPPUP_TARGET_ARCH=x86_64 CACHE STRING "Target macOS architecture (x86_64, arm64)"
set(CPPUP_TARGET_ARCH "arm64")
if(APPLE AND CPPUP_TARGET_ARCH)
  set(CMAKE_OSX_ARCHITECTURES "${CPPUP_TARGET_ARCH}")
endif()

project(cppup VERSION "${CPPUP_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prefer Ninja if available
if(NOT CMAKE_GENERATOR)
  find_program(NINJA_EXECUTABLE ninja)
  if(NINJA_EXECUTABLE)
    set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)
  endif()
endif()

add_library(cppup_core
  src/core/project.cpp
  src/core/modules.cpp
  src/platform/env.cpp
  src/templates/templates.cpp
  src/util/util.cpp
)

target_include_directories(cppup_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_features(cppup_core PUBLIC cxx_std_23)

add_executable(cppup
  src/app/main.cpp
  src/cli/commands.cpp
)
target_link_libraries(cppup PRIVATE cppup_core)
target_compile_features(cppup PRIVATE cxx_std_23)

if(CPPUP_ENABLE_CXX_MODULES)
  set_property(TARGET cppup_core PROPERTY CXX_MODULE_STD ON)
  set_property(TARGET cppup PROPERTY CXX_MODULE_STD ON)
endif()

# Install
install(TARGETS cppup RUNTIME DESTINATION bin)

# Optional: install docs
install(FILES README.md DESTINATION share/doc/cppup)
install(FILES docs/USAGE.md DESTINATION share/doc/cppup)