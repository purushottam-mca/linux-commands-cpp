add_executable(mycat main.cpp)
target_link_libraries(mycat PRIVATE common)
target_include_directories(mycat PRIVATE ${CMAKE_SOURCE_DIR}/src/common)

# Ensure strict C++20
target_compile_features(mycat PRIVATE cxx_std_20)

# Output bin to build root for convenience
set_target_properties(mycat PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)