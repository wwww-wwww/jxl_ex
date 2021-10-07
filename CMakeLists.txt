cmake_minimum_required(VERSION 3.10)
project(jxl_ex_nif)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY $ENV{MIX_COMPILE_PATH}/../priv)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY $ENV{MIX_COMPILE_PATH}/../priv)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $ENV{MIX_COMPILE_PATH}/../priv)

set(JPEGXL_ENABLE_TOOLS OFF CACHE INTERNAL
    "Build JPEGXL user tools: cjxl and djxl.")
set(JPEGXL_ENABLE_MANPAGES OFF CACHE INTERNAL
    "Build and install man pages for the command-line tools.")
set(JPEGXL_ENABLE_BENCHMARK OFF CACHE INTERNAL
    "Build JPEGXL benchmark tools.")
set(JPEGXL_ENABLE_EXAMPLES OFF CACHE INTERNAL
    "Build JPEGXL library usage examples.")
set(JPEGXL_ENABLE_JNI OFF CACHE INTERNAL
    "Build JPEGXL JNI Java wrapper, if Java dependencies are installed.")
set(JPEGXL_ENABLE_OPENEXR OFF CACHE INTERNAL
    "Build JPEGXL with support for OpenEXR if available.")

option(BUILD_TESTING "" OFF)

add_subdirectory(libjxl)

add_library(jxl_ex_nif cpp_src/jxl_nif.cpp cpp_src/jxl_dec_nif.cpp)
target_link_libraries(jxl_ex_nif jxl_dec-static jxl_threads-static)
target_include_directories(jxl_ex_nif PRIVATE
  ${libjxl_BINARY_DIR}
  ${libjxl_SOURCE_DIR}
  $ENV{ERTS_INCLUDE_DIR}
)
