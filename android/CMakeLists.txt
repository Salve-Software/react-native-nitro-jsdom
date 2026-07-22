project(NitroJsdom)
cmake_minimum_required(VERSION 3.9.0)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set(CMAKE_C_COMPILER_LAUNCHER ccache)
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

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
add_subdirectory(${CMAKE_SOURCE_DIR}/../packages/lexbor lexbor_build)

# ---------------------------------------------------------------------------
# Third-party: QuickJS
# ---------------------------------------------------------------------------
set(QUICKJS_SOURCES
  ${CMAKE_SOURCE_DIR}/../packages/quickjs/quickjs.c
  ${CMAKE_SOURCE_DIR}/../packages/quickjs/libregexp.c
  ${CMAKE_SOURCE_DIR}/../packages/quickjs/libunicode.c
  ${CMAKE_SOURCE_DIR}/../packages/quickjs/cutils.c
  ${CMAKE_SOURCE_DIR}/../packages/quickjs/dtoa.c
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
  ../cpp/quickjs/DOMBindingsInternal.cpp
  ../cpp/quickjs/MutationObservers.cpp
  ../cpp/quickjs/Storage.cpp
  ../cpp/quickjs/bindings/ClassListBindings.cpp
  ../cpp/quickjs/bindings/DatasetBindings.cpp
  ../cpp/quickjs/bindings/DOMExceptionBindings.cpp
  ../cpp/quickjs/bindings/StyleBindings.cpp
  ../cpp/quickjs/bindings/ElementBindings.cpp
  ../cpp/quickjs/bindings/DocumentBindings.cpp
  ../cpp/quickjs/bindings/EventBindings.cpp
  ../cpp/quickjs/bindings/TimerBindings.cpp
  ../cpp/quickjs/bindings/WindowBindings.cpp
  ../cpp/quickjs/bindings/FetchBindings.cpp
  ../cpp/quickjs/bindings/LiveCollectionBindings.cpp
  ../cpp/quickjs/bindings/UrlBindings.cpp
  ../cpp/quickjs/bindings/AbortBindings.cpp
  ../cpp/quickjs/bindings/TextEncodingBindings.cpp
  ../cpp/quickjs/bindings/FormBindings.cpp
  ../cpp/quickjs/bindings/BlobBindings.cpp
  ../cpp/quickjs/bindings/CSSOMBindings.cpp
  ../cpp/quickjs/bindings/ShadowRootBindings.cpp
  ${QUICKJS_SOURCES}
)

target_compile_definitions(${PACKAGE_NAME} PRIVATE CONFIG_VERSION="2025-09-13")

# Add Nitrogen specs :)
include(${CMAKE_SOURCE_DIR}/../nitrogen/generated/android/NitroJsdom+autolinking.cmake)

# Set up local includes
include_directories(
  "src/main/cpp"
  "../cpp"
  "${CMAKE_SOURCE_DIR}/../packages/lexbor/source"
)

# Use -iquote for QuickJS so packages/quickjs/VERSION never shadows <version> (C++20)
target_compile_options(${PACKAGE_NAME} PRIVATE
  "-iquote${CMAKE_SOURCE_DIR}/../packages/quickjs"
)

find_library(LOG_LIB log)

# Link all libraries together
target_link_libraries(
  ${PACKAGE_NAME}
  ${LOG_LIB}
  android                                   # <-- Android core
  lexbor_static
)
