cmake_minimum_required(VERSION 3.14.0)
project(verona_dyn VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

# Used to provide
#  FetchContent_MakeAvailable_ExcludeFromAll
FetchContent_Declare(
    cmake_utils
    GIT_REPOSITORY https://github.com/mjp41/cmake_utils
    GIT_TAG 2bf98b5773ea7282197c823e205547d8c2e323c0
    GIT_SHALLOW FALSE
)

FetchContent_MakeAvailable(cmake_utils)

FetchContent_Declare(
  trieste
  GIT_REPOSITORY https://github.com/microsoft/trieste
  GIT_TAG 467157a167d2e929095498089f29fc9419f4ef11
  GIT_SHALLOW FALSE
)
set (TRIESTE_BUILD_SAMPLES OFF)
set (TRIESTE_BUILD_PARSERS OFF)
set (TRIESTE_ENABLE_TESTING OFF)
FetchContent_MakeAvailable_ExcludeFromAll(trieste)
set(CMAKE_CXX_STANDARD 20)

if(SANITIZER)
  message(STATUS "Run-time sanitizer=${SANITIZER}")
endif()
macro(add_san target)
  # ASAN must be added first, or it gets upset.
  if(SANITIZER)
    if(MSVC)
      message(FATAL_ERROR "MSVC does not support sanitizers")
    endif()
    target_compile_definitions(${target} INTERFACE -DSNMALLOC_PASS_THROUGH)
    target_compile_options(${target} INTERFACE -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer)
    target_link_libraries(${target} INTERFACE -fsanitize=${SANITIZER})
  endif()
endmacro()

add_library(
  rt OBJECT
  src/rt/rt.cc
  src/rt/objects/region.cc
  src/rt/ui/mermaid.cc
  src/rt/core/builtin.cc
)
add_san(rt)

add_library(
  lang OBJECT
  src/lang/lang.cc
  src/lang/interpreter.cc
  src/lang/passes/parse.cc
  src/lang/passes/grouping.cc
  src/lang/passes/call_stmts.cc
  src/lang/passes/flatten.cc
  src/lang/passes/bytecode.cc
)
add_san(lang)

target_link_libraries(lang PRIVATE trieste::trieste)

add_executable(verona_dyn src/main.cc)
add_san(verona_dyn)

target_link_libraries(verona_dyn PRIVATE rt lang)

set_property(TARGET verona_dyn PROPERTY COMPILE_WARNING_AS_ERROR ON)

# Add snmallocs clang format targets
clangformat_targets()


message(STATUS "Adding tests")
FILE(GLOB_RECURSE ALL_FILES
  CONFIGURE_DEPENDS
  tests/*
)

enable_testing()
foreach(FILE ${ALL_FILES})
  get_filename_component(FILENAME ${FILE} NAME)
  message(STATUS "Adding test: ${FILENAME} -- ${FILE}")
  add_test(NAME ${FILENAME} COMMAND verona_dyn build ${FILE})
endforeach()

set_property(TEST three_regions.vpy PROPERTY WILL_FAIL true)
set_property(TEST leak_with_global.vpy PROPERTY WILL_FAIL true)
set_property(TEST invalid_read.vpy PROPERTY WILL_FAIL true)
set_property(TEST invalid_shared_region.vpy PROPERTY WILL_FAIL true)
set_property(TEST invalid_write.vpy PROPERTY WILL_FAIL true)
