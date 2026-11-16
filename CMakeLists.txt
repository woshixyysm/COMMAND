cmake_minimum_required(VERSION 3.20)
project(commandpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(commandpp
    src/platform_common.cpp
    src/env.cpp
    src/text.cpp
    src/shell.cpp
    src/commands.cpp
    src/commands_file.cpp
    src/commands_copy.cpp
    src/help.cpp
    src/main.cpp)

if(WIN32)
    target_sources(commandpp PRIVATE src/platform_win.cpp)
else()
    target_sources(commandpp PRIVATE src/platform_posix.cpp)
endif()

target_include_directories(commandpp PRIVATE include src)

if(MSVC)
    target_compile_options(commandpp PRIVATE /W4)
else()
    target_compile_options(commandpp PRIVATE -Wall -Wextra)
endif()
