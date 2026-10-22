cmake_minimum_required(VERSION 4.0)
project(quickheap VERSION 0.2.0 LANGUAGES C CXX)

if(NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 20)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(QUICKHEAP_SCALAR "Build scalar Highway kernels only" OFF)
option(QUICKHEAP_BUILD_BENCHMARKS "Build and run benchmarks" OFF)

find_package(highway 1.4.0 REQUIRED CONFIG)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_link_options(-Wl,--fatal-warnings)
    endif()
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(test)
elseif(QUICKHEAP_BUILD_BENCHMARKS)
    message(FATAL_ERROR "Benchmarks require BUILD_TESTING=ON")
endif()

configure_package_config_file(
    cmake/quickheap-config.cmake.in
    ${PROJECT_BINARY_DIR}/quickheap-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickheap
)
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/quickheap-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT quickheapTargets
    FILE quickheapTargets.cmake
    NAMESPACE quickheap::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickheap
)
install(FILES
    ${PROJECT_BINARY_DIR}/quickheap-config.cmake
    ${PROJECT_BINARY_DIR}/quickheap-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickheap
)
