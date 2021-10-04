cmake_minimum_required(VERSION 3.14)
project(jxl_ex_nif)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY $ENV{MIX_COMPILE_PATH}/../priv)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY $ENV{MIX_COMPILE_PATH}/../priv)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $ENV{MIX_COMPILE_PATH}/../priv)

include(FetchContent)
add_subdirectory(libjxl)

add_library(jxl_ex_nif cpp_src/jxl_nif.cpp cpp_src/jxl_dec_nif.cpp)
target_link_libraries(jxl_ex_nif jxl_dec-static jxl_threads-static)
target_include_directories(jxl_ex_nif PRIVATE
  ${libjxl_BINARY_DIR}
  ${libjxl_SOURCE_DIR}
  $ENV{ERTS_INCLUDE_DIR}
)
