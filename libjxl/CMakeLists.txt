FetchContent_Declare(libjxl
  GIT_REPOSITORY  https://github.com/libjxl/libjxl
  GIT_TAG         c31dca74e68f6d1550692a2e28780499fa4e7dd5
  PATCH_COMMAND   git apply
    ${CMAKE_CURRENT_LIST_DIR}/01-skcms-directory.patch || true
  BINARY_DIR      build
  SUBBUILD_DIR    subbuild
)

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

FetchContent_MakeAvailable(libjxl)
