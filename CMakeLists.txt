cmake_minimum_required(VERSION 3.16)
project(linux-commands-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)

# Always emit build/compile_commands.json so clangd / C/C++ IntelliSense
# resolve headers (including src/common/*) correctly.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# Common header-only library
add_library(common INTERFACE)
target_include_directories(common INTERFACE src/common)

# Commands
add_subdirectory(src/cat)
add_subdirectory(src/ls)
add_subdirectory(src/chmod)

enable_testing()
