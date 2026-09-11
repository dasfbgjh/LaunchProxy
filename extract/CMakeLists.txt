cmake_minimum_required(VERSION 3.20)
project(extract)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(extract
    extract.cpp
    extract_ico.hpp
)
target_compile_definitions(extract PRIVATE
    -DEXE_FILE="${EXE_FILE}"
    -DINFO_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../win/WinInfo.h"
    -DMANIFEST_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../win/winManifest.manifest"
    -DICO_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../win/winLogo.ico"
)

add_custom_command(
    TARGET extract POST_BUILD
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/extract
)
