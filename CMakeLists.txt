cmake_minimum_required(VERSION 4.0)
project(quickheap VERSION 0.2.0 LANGUAGES C CXX)

if(NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 20)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(QUICKHEAP_SCALAR "Build scalar Highway kernels only" OFF)
option(QUICKHEAP_BUILD_BENCHMARKS "Build and run benchmarks" OFF)

find_package(highway 1.4.0 REQUIRED CONFIG)

add_library(quickheap src/quickheap.cpp src/simd.cpp)
add_library(quickheap::quickheap ALIAS quickheap)
set_target_properties(quickheap PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME quickheap
)
target_compile_features(quickheap PRIVATE cxx_std_20)
target_include_directories(quickheap
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(quickheap PRIVATE highway::hwy)
target_compile_definitions(quickheap PRIVATE QUICKHEAP_BUILDING_LIBRARY=1)
target_compile_definitions(quickheap PUBLIC
    $<$<BOOL:${BUILD_SHARED_LIBS}>:QUICKHEAP_SHARED=1>)
if(QUICKHEAP_SCALAR)
    target_compile_definitions(quickheap PRIVATE HWY_COMPILE_ONLY_SCALAR=1)
else()
    target_compile_definitions(quickheap PRIVATE HWY_COMPILE_ALL_ATTAINABLE=1)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(quickheap PRIVATE -Wall -Wextra -Werror)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(quickheap PRIVATE -Wl,--fatal-warnings)
    endif()
endif()

if(BUILD_TESTING)
    find_package(GTest REQUIRED CONFIG)
    add_executable(quickheap_tests
        test/quickheap_test.cpp
        test/c_api_test.c
    )
    target_link_libraries(quickheap_tests PRIVATE quickheap GTest::gtest_main)
    add_test(NAME quickheap_tests COMMAND quickheap_tests)
    set_tests_properties(quickheap_tests PROPERTIES
        ENVIRONMENT_MODIFICATION
            "ASAN_OPTIONS=set:halt_on_error=1:detect_leaks=1;UBSAN_OPTIONS=set:halt_on_error=1:print_stacktrace=1;TSAN_OPTIONS=set:halt_on_error=1")

    add_executable(quickheap_trace test/trace.cpp)
    target_link_libraries(quickheap_trace PRIVATE quickheap)
endif()

if(QUICKHEAP_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED CONFIG)
    add_executable(quickheap_benchmark benchmark/quickheap_benchmark.cpp)
    target_link_libraries(quickheap_benchmark PRIVATE
        quickheap benchmark::benchmark_main)
    if(BUILD_TESTING)
        add_test(NAME quickheap_benchmark
            COMMAND quickheap_benchmark
                --benchmark_filter=/1024$
                --benchmark_min_time=0.001s
                --benchmark_repetitions=1)
    endif()
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

install(TARGETS quickheap EXPORT quickheapTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    include/quickheap.h
    include/quickheap.hpp
    include/key_value.hpp
    include/export.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
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
