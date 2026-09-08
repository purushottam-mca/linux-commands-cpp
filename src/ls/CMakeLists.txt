add_executable(myls main.cpp)
target_link_libraries(myls PRIVATE common)
target_include_directories(myls PRIVATE ${CMAKE_SOURCE_DIR}/src/common)

# Ensure strict C++20
target_compile_features(myls PRIVATE cxx_std_20)

# Output bin to build root for convenience
set_target_properties(myls PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)