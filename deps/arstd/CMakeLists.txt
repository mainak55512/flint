cmake_minimum_required(VERSION 3.16)

project(arstd
    VERSION 0.1.0
    DESCRIPTION "Arena-backed standard library for C (Arena, Vector, Cmap, String)"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON) 

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Collect library source files
set(ARSTD_SOURCES
    lib/arena.c
    lib/cvector.c
    lib/cmap.c
    lib/cstring.c
)

add_library(arstd STATIC ${ARSTD_SOURCES})

add_library(arstd::arstd ALIAS arstd)

target_include_directories(arstd
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(arstd PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
