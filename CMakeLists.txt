cmake_minimum_required(VERSION 4.0)
project(quickheap LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CTest)

option(QUICKHEAP_SCALAR "Build scalar Highway kernels only" OFF)

find_package(highway REQUIRED CONFIG)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_link_options(-Wl,--fatal-warnings)
    endif()
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
