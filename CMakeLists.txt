cmake_minimum_required(VERSION 3.16)
project(linux-commands-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)

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

enable_testing()
