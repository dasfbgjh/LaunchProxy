cmake_minimum_required(VERSION 3.10)
project(
    LauncherProxy
    VERSION 1.0.0
    DESCRIPTION "launcher proxy"
    HOMEPAGE_URL "https://example.com"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXE_FILE "C:/test.exe")

add_subdirectory(extract)

add_executable(LauncherProxy WIN32
    win/winRC.rc
    src/main.cpp
    src/info.hpp
)
target_link_options(LauncherProxy PRIVATE
    -static-libgcc
    -static-libstdc++
)
target_include_directories(LauncherProxy PRIVATE win)

add_dependencies(LauncherProxy extract)
