cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h" CPPUP_VERSION_LINE
  REGEX "^#define CPPUP_VERSION ")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" CPPUP_VERSION "${CPPUP_VERSION_LINE}")
if(NOT CPPUP_VERSION)
  message(FATAL_ERROR "Could not read CPPUP_VERSION from src/version.h")
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
  src/platform/env.cpp
  src/templates/templates.cpp
  src/util/util.cpp
)

target_include_directories(cppup_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(cppup
  src/app/main.cpp
  src/cli/commands.cpp
)
target_link_libraries(cppup PRIVATE cppup_core)

# Install
install(TARGETS cppup RUNTIME DESTINATION bin)

# Optional: install docs
install(FILES README.md DESTINATION share/doc/cppup)
install(FILES docs/USAGE.md DESTINATION share/doc/cppup)