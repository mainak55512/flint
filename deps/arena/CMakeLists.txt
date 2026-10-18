# CMake version
cmake_minimum_required(VERSION 4.1.2)

set(CMAKE_C_COMPILER "/usr/bin/clang-18" CACHE PATH "" FORCE)

project(arena)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC lib/arena.c)

target_include_directories(${PROJECT_NAME} PRIVATE include)

