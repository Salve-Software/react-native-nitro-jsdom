project(NitroJsdom)
cmake_minimum_required(VERSION 3.9.0)

set (PACKAGE_NAME NitroJsdom)
set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 20)

# Enable Raw Props parsing in react-native (for Nitro Views)
add_compile_options(-DRN_SERIALIZABLE_STATE=1)

# ---------------------------------------------------------------------------
# Third-party: Lexbor
# ---------------------------------------------------------------------------
set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/../third_party/lexbor lexbor_build)

# ---------------------------------------------------------------------------
# Third-party: QuickJS
# ---------------------------------------------------------------------------
set(QUICKJS_SOURCES
  ${CMAKE_SOURCE_DIR}/../third_party/quickjs/quickjs.c
  ${CMAKE_SOURCE_DIR}/../third_party/quickjs/libregexp.c
  ${CMAKE_SOURCE_DIR}/../third_party/quickjs/libunicode.c
  ${CMAKE_SOURCE_DIR}/../third_party/quickjs/cutils.c
)

set_source_files_properties(${QUICKJS_SOURCES} PROPERTIES
  COMPILE_FLAGS "-Wno-implicit-fallthrough -Wno-unused-variable -Wno-sign-compare"
)

# Define C++ library and add all sources
add_library(${PACKAGE_NAME} SHARED
  src/main/cpp/cpp-adapter.cpp
  ../cpp/HybridHtmlSandbox.cpp
  ../cpp/lexbor/LexborDocument.cpp
  ../cpp/quickjs/QuickJSRuntime.cpp
  ../cpp/quickjs/DOMBindings.cpp
  ${QUICKJS_SOURCES}
)

target_compile_definitions(${PACKAGE_NAME} PRIVATE CONFIG_VERSION="2024-01-13")

# Add Nitrogen specs :)
include(${CMAKE_SOURCE_DIR}/../nitrogen/generated/android/NitroJsdom+autolinking.cmake)

# Set up local includes
include_directories(
  "src/main/cpp"
  "../cpp"
  "${CMAKE_SOURCE_DIR}/../third_party/lexbor/source"
  "${CMAKE_SOURCE_DIR}/../third_party/quickjs"
)

find_library(LOG_LIB log)

# Link all libraries together
target_link_libraries(
  ${PACKAGE_NAME}
  ${LOG_LIB}
  android                                   # <-- Android core
  lexbor_static
)
