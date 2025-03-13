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

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
# set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

add_library(
  rt OBJECT
  src/rt/rt.cc
  src/rt/objects/region.cc
  src/rt/ui/mermaid.cc
  src/rt/core/builtin.cc
)

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
target_link_libraries(lang PRIVATE trieste::trieste)

add_executable(frankenscript src/main.cc)
target_link_libraries(frankenscript PRIVATE rt lang)

set_property(TARGET frankenscript PROPERTY COMPILE_WARNING_AS_ERROR ON)

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
  add_test(NAME ${FILENAME} COMMAND frankenscript build ${FILE})
endforeach()

set_property(TEST three_regions.frank PROPERTY WILL_FAIL true)
set_property(TEST leak_with_global.frank PROPERTY WILL_FAIL true)
set_property(TEST invalid_read.frank PROPERTY WILL_FAIL true)
set_property(TEST invalid_write.frank PROPERTY WILL_FAIL true)
set_property(TEST invalid_child_region.frank PROPERTY WILL_FAIL true)
set_property(TEST invalid_not_bridge.frank PROPERTY WILL_FAIL true)
set_property(TEST region_bad_1.frank PROPERTY WILL_FAIL true)
set_property(TEST region_bad_2.frank PROPERTY WILL_FAIL true)
set_property(TEST fail_cross_region_ref.frank PROPERTY WILL_FAIL true)
set_property(TEST region_close_fail_1.frank PROPERTY WILL_FAIL true)
set_property(TEST merge_bad_1.frank PROPERTY WILL_FAIL true)
set_property(TEST merge_bad_2.frank PROPERTY WILL_FAIL true)
set_property(TEST merge_bad_3.frank PROPERTY WILL_FAIL true)
set_property(TEST dissolve_bad_1.frank PROPERTY WILL_FAIL true)
set_property(TEST dissolve_bad_2.frank PROPERTY WILL_FAIL true)
set_property(TEST frozen_bridge_segfault.frank PROPERTY WILL_FAIL true)
set_property(TEST unresolved_field.frank PROPERTY WILL_FAIL true)
set_property(TEST unresolved_global.frank PROPERTY WILL_FAIL true)
set_property(TEST unresolved_name.frank PROPERTY WILL_FAIL true)
