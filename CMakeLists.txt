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
set(JPEGXL_ENABLE_SKCMS OFF CACHE INTERNAL
    "Build with skcms instead of lcms2.")
set(JPEGXL_BUNDLE_SKCMS OFF CACHE INTERNAL
    "When building with skcms, bundle it into libjxl.a.")

option(BUILD_TESTING "" OFF)

add_subdirectory(libjxl EXCLUDE_FROM_ALL)

get_target_property(JPEGXL_INTERNAL_SOURCES_DEC jxl_dec-obj SOURCES)
get_target_property(JPEGXL_INTERNAL_SOURCES_ENC jxl_enc-obj SOURCES)
set(JPEGXL_INTERNAL_SOURCES ${JPEGXL_INTERNAL_SOURCES_DEC} ${JPEGXL_INTERNAL_SOURCES_ENC})
list(REMOVE_ITEM JPEGXL_INTERNAL_SOURCES jxl/encode.cc)
list(REMOVE_ITEM JPEGXL_INTERNAL_SOURCES jxl/decode.cc)
list(REMOVE_DUPLICATES JPEGXL_INTERNAL_SOURCES)
list(TRANSFORM JPEGXL_INTERNAL_SOURCES PREPEND ${LIBJXL_SOURCE_DIR}/lib/)

add_library(jxl_ex_nif SHARED
  cpp_src/jxl_nif.cpp
  cpp_src/jxl_dec_nif.cpp
  cpp_src/jxl_enc_nif.cpp
  ${JPEGXL_INTERNAL_SOURCES}
)

target_link_libraries(jxl_ex_nif
  jxl_dec-static
  jxl_threads-static
  brotlidec-static
  brotlienc-static
  brotlicommon-static
  lcms2
  hwy
  -Wl,--allow-multiple-definition
)

target_include_directories(jxl_ex_nif PRIVATE
  $ENV{ERTS_INCLUDE_DIR}
  ${LIBJXL_SOURCE_DIR}
  $<TARGET_PROPERTY:hwy,INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:lcms2,INCLUDE_DIRECTORIES>
)
