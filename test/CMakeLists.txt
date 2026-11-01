cmake_minimum_required(VERSION 3.30)

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

project(Cpp23ModuleStarter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# libc++ module support has historically required GNU extensions to be
# enabled with CMake's import-std integration. Keep this ON for compatibility
# with that workflow; revisit when your exact toolchain documents otherwise.
set(CMAKE_CXX_EXTENSIONS ON)

add_executable(app)

target_sources(app
    PRIVATE
        src/main.cpp
    PRIVATE
        FILE_SET CXX_MODULES
        FILES
            modules/app.math.cppm
            modules/app.service.cppm
)

# Request CMake's standard-library module dependency where supported.
# This property only works when the current CMake version has import-std
# support enabled and the selected toolchain provides the required module.
set_property(TARGET app PROPERTY CXX_MODULE_STD ON)

# Use libc++ explicitly. Keep this consistent with the compiler and the
# standard-library BMI configuration.
target_compile_options(app PRIVATE -stdlib=libc++)
target_link_options(app PRIVATE -stdlib=libc++)

# Optional, helpful diagnostics during initial setup.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(app PRIVATE -Wno-reserved-module-identifier)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_custom_target(copy_module_bmis ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/modules"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/modules"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/CMakeFiles/app.dir/app.math.pcm"
            "${CMAKE_SOURCE_DIR}/modules/app.math.pcm"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/CMakeFiles/app.dir/app.service.pcm"
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
        DEPENDS app
        VERBATIM
    )
endif()
