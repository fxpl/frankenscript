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
  GIT_TAG b466068270471ccc9c5f5ddd543bd6e2fb02ad87
  GIT_SHALLOW FALSE
)

FetchContent_MakeAvailable_ExcludeFromAll(trieste)

set(CMAKE_CXX_STANDARD 20)
add_executable(verona_dyn src/main.cc src/rt/rt.cc)