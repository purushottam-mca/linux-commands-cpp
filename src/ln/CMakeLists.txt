add_executable(myln main.cpp)
target_link_libraries(myln PRIVATE common)
target_include_directories(myln PRIVATE ${CMAKE_SOURCE_DIR}/src/common)

# Ensure strict C++20
target_compile_features(myln PRIVATE cxx_std_20)

# Output bin to build root for convenience
set_target_properties(myln PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
