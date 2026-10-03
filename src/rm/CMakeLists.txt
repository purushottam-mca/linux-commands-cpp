add_executable(myrm main.cpp)
target_link_libraries(myrm PRIVATE common)
target_include_directories(myrm PRIVATE ${CMAKE_SOURCE_DIR}/src/common)

# Ensure strict C++20
target_compile_features(myrm PRIVATE cxx_std_20)

# Output bin to build root for convenience
set_target_properties(myrm PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
