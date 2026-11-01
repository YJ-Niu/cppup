cmake_minimum_required(VERSION 3.20)
project(cppup VERSION 0.1.0 LANGUAGES CXX)

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
  src/project.cpp
  src/env.cpp
  src/templates.cpp
  src/util.cpp
)

target_include_directories(cppup_core PUBLIC src)

add_executable(cppup
  src/main.cpp
  src/commands.cpp
)
target_link_libraries(cppup PRIVATE cppup_core)

# Install
install(TARGETS cppup RUNTIME DESTINATION bin)

# Optional: install docs
install(FILES README.md DESTINATION share/doc/cppup)
install(FILES docs/USAGE.md DESTINATION share/doc/cppup)