cmake_minimum_required(VERSION 4.0)
project(quickheap_bench LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(QUICKHEAP_SCALAR "Build scalar benchmark kernels only" OFF)
option(QUICKHEAP_BENCH_BOOST "Build Boost benchmark competitors" OFF)

find_package(quickheap REQUIRED CONFIG)
find_package(highway REQUIRED CONFIG)
find_package(Catch2 3.16 REQUIRED CONFIG)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_link_options(-Wl,--fatal-warnings)
    endif()
endif()

add_executable(quickheap_benchmark quickheap_benchmark.cpp)
target_link_libraries(quickheap_benchmark PRIVATE quickheap::quickheap Catch2::Catch2WithMain)

add_library(quickheap_bench_graph_support STATIC graph.cpp)
target_link_libraries(quickheap_bench_graph_support PUBLIC quickheap::quickheap)
target_include_directories(quickheap_bench_graph_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(quickheap_bench_graph bench_graph.cpp)
target_link_libraries(quickheap_bench_graph PRIVATE quickheap_bench_graph_support)

add_executable(quickheap_bench bench.cpp)
target_link_libraries(quickheap_bench PRIVATE quickheap::quickheap)

add_library(quickheap_bench_backend STATIC backend.cpp experiment_simd.cpp simd.cpp)
target_link_libraries(quickheap_bench_backend PUBLIC highway::hwy)
target_include_directories(quickheap_bench_backend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(QUICKHEAP_SCALAR)
    target_compile_definitions(quickheap_bench_backend PRIVATE HWY_COMPILE_ONLY_SCALAR=1)
else()
    target_compile_definitions(quickheap_bench_backend PRIVATE HWY_COMPILE_ALL_ATTAINABLE=1)
endif()
add_executable(quickheap_experiments experiments.cpp)
target_link_libraries(quickheap_experiments PRIVATE quickheap::quickheap quickheap_bench_backend)
target_link_libraries(quickheap_bench PRIVATE quickheap_bench_backend)
target_link_libraries(quickheap_bench_graph PRIVATE quickheap_bench_backend)

if(BUILD_TESTING)
    add_executable(quickheap_bench_tests graph_test.cpp workload_test.cpp experimental_heap_test.cpp competitor_test.cpp)
    target_link_libraries(quickheap_bench_tests PRIVATE quickheap_bench_graph_support Catch2::Catch2WithMain)
    target_link_libraries(quickheap_bench_tests PRIVATE quickheap_bench_backend)
endif()

add_subdirectory(external)
if(TARGET quickheap_bench_external)
    target_link_libraries(quickheap_bench PRIVATE quickheap_bench_external)
    target_link_libraries(quickheap_bench_graph PRIVATE quickheap_bench_external)
    if(BUILD_TESTING)
        target_sources(quickheap_bench_tests PRIVATE external_test.cpp)
        target_link_libraries(quickheap_bench_tests PRIVATE quickheap_bench_external)
    endif()
endif()

if(QUICKHEAP_BENCH_BOOST)
    find_package(Boost REQUIRED CONFIG)
    target_link_libraries(quickheap_bench PRIVATE Boost::headers)
    target_link_libraries(quickheap_bench_graph PRIVATE Boost::headers)
    target_compile_definitions(quickheap_bench PRIVATE QUICKHEAP_BENCH_BOOST=1)
    target_compile_definitions(quickheap_bench_graph PRIVATE QUICKHEAP_BENCH_BOOST=1)
    if(BUILD_TESTING)
        target_link_libraries(quickheap_bench_tests PRIVATE Boost::headers)
        target_compile_definitions(quickheap_bench_tests PRIVATE QUICKHEAP_BENCH_BOOST=1)
    endif()
endif()
if(BUILD_TESTING)
    add_test(NAME quickheap_benchmark
        COMMAND quickheap_benchmark
            "[benchmark]"
            --section 1024
            --benchmark-samples 1
            --benchmark-warmup-time 0
            --benchmark-no-analysis)
    add_test(NAME quickheap_bench_contracts COMMAND quickheap_bench_tests)
    add_test(NAME quickheap_graph_smoke
        COMMAND quickheap_bench_graph ${CMAKE_CURRENT_SOURCE_DIR}/data/smoke.gr --repeat 1)
    add_test(NAME quickheap_native_smoke
        COMMAND quickheap_bench --min 5 --max 5 --repeat 1 --i32 --i64 --u32 --u64)
    add_test(NAME quickheap_graph_zero_weight
        COMMAND quickheap_bench_graph ${CMAKE_CURRENT_SOURCE_DIR}/data/zero-weight.gr --repeat 1)
    foreach(mode variants ablation comparisons rebalancing partition)
        add_test(NAME quickheap_experiment_${mode}
            COMMAND quickheap_experiments --mode ${mode} --min 5 --max 5 --repeat 1
                --i32 --i64 --u32 --u64)
    endforeach()
endif()

install(TARGETS quickheap_bench quickheap_bench_graph quickheap_experiments quickheap_benchmark
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS run-all.sh DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench)
install(DIRECTORY data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench/data
    FILES_MATCHING PATTERN "*.gr")
install(DIRECTORY evals/ DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench/evals
    FILES_MATCHING PATTERN "*.py")
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench)
