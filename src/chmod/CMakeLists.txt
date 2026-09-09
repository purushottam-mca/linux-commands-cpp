add_executable(mychmod main.cpp)
target_link_libraries(mychmod PRIVATE common)
target_include_directories(mychmod PRIVATE ${CMAKE_SOURCE_DIR}/src/common)

# Ensure strict C++20
target_compile_features(mychmod PRIVATE cxx_std_20)

# Output bin to build root for convenience
set_target_properties(mychmod PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)