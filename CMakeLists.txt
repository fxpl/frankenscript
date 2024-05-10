cmake_minimum_required(VERSION 3.14.0)
project(verona_dyn VERSION 1.0.0 LANGUAGES CXX)

# FetchContent_Declare(
#   trieste
#   GIT_REPOSITORY https://github.com/microsoft/trieste
#   GIT_TAG b466068270471ccc9c5f5ddd543bd6e2fb02ad87
#   GIT_SHALLOW FALSE
# )

set(CMAKE_CXX_STANDARD 20)
add_executable(verona_dyn src/main.cc src/rt/rt.cc)