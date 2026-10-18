# CMake version
cmake_minimum_required(VERSION 4.1.2)

set(CMAKE_C_COMPILER "/usr/bin/clang-18" CACHE PATH "" FORCE)

project(ccontainer)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)


add_library(${PROJECT_NAME} STATIC lib/cvector.c)
target_include_directories(${PROJECT_NAME} PRIVATE include)
