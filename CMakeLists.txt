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

add_library(rt OBJECT src/rt/rt.cc)

add_library(lang OBJECT src/lang/lang.cc src/lang/interpreter.cc)
target_link_libraries(lang PRIVATE trieste::trieste)

add_executable(verona_dyn src/main.cc)
target_link_libraries(verona_dyn PRIVATE rt lang)

# Add snmallocs clang format targets
clangformat_targets()


message(STATUS "Adding tests")
FILE(GLOB ALL_FILES
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
