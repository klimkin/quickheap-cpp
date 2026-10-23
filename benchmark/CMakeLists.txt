cmake_minimum_required(VERSION 4.0)
project(quickheap_bench LANGUAGES CXX)

include(GNUInstallDirs)
include(CTest)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(QUICKHEAP_SCALAR "Build scalar benchmark kernels only" OFF)
option(QUICKHEAP_BENCH_BOOST "Build Boost benchmark competitors" OFF)
option(QUICKHEAP_BENCH_FIXED_TARGETS "Build independent SIMD benchmark executables" OFF)

find_package(quickheap REQUIRED CONFIG)
find_package(highway REQUIRED CONFIG)

if(NOT DEFINED QUICKHEAP_BENCH_LIBRARY_SCALAR)
    message(FATAL_ERROR "Set QUICKHEAP_BENCH_LIBRARY_SCALAR to the imported Quickheap package scalar option, or configure through Conan")
endif()
if((QUICKHEAP_SCALAR AND NOT QUICKHEAP_BENCH_LIBRARY_SCALAR) OR
   (QUICKHEAP_BENCH_LIBRARY_SCALAR AND NOT QUICKHEAP_SCALAR))
    message(FATAL_ERROR "Benchmark and imported Quickheap scalar options must match")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_link_options(-Wl,--fatal-warnings)
    endif()
endif()

add_library(quickheap_bench_graph_support STATIC graph.cpp)
target_link_libraries(quickheap_bench_graph_support PUBLIC quickheap::quickheap)
target_include_directories(quickheap_bench_graph_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(quickheap_bench_graph bench_graph.cpp)
target_link_libraries(quickheap_bench_graph PRIVATE quickheap_bench_graph_support)

add_executable(quickheap_bench bench.cpp)
target_link_libraries(quickheap_bench PRIVATE quickheap::quickheap)

add_executable(quickheap_fixture fixture.cpp)
target_link_libraries(quickheap_fixture PRIVATE quickheap::quickheap)

file(GLOB benchmark_sources CONFIGURE_DEPENDS "*.cpp" "*.hpp" "*.hpp.in")
file(GLOB external_sources CONFIGURE_DEPENDS "external/*.cpp" "external/*.hpp" "external/*.cmake" "external/CMakeLists.txt")
list(APPEND benchmark_sources ${external_sources})
list(APPEND benchmark_sources "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/conanfile.py" "${CMAKE_CURRENT_SOURCE_DIR}/protocol.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${benchmark_sources})
set(benchmark_source_hashes "")
foreach(source IN LISTS benchmark_sources)
    file(SHA256 "${source}" source_hash)
    string(APPEND benchmark_source_hashes "${source_hash}")
endforeach()
string(SHA256 QUICKHEAP_BENCH_SOURCE_DIGEST "${benchmark_source_hashes}")
string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper)
set(QUICKHEAP_BENCH_BUILD "${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}/${CMAKE_BUILD_TYPE}/${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${build_type_upper}}")
target_include_directories(quickheap_bench PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

set(dependency_hashes "")
foreach(dependency quickheap highway)
    foreach(config RELEASE DEBUG RELWITHDEBINFO MINSIZEREL)
        if(DEFINED ${dependency}_PACKAGE_FOLDER_${config})
            file(GLOB_RECURSE dependency_files CONFIGURE_DEPENDS
                "${${dependency}_PACKAGE_FOLDER_${config}}/include/*"
                "${${dependency}_PACKAGE_FOLDER_${config}}/lib/*")
            foreach(dependency_file IN LISTS dependency_files)
                if(NOT IS_DIRECTORY "${dependency_file}")
                    file(SHA256 "${dependency_file}" dependency_hash)
                    string(APPEND dependency_hashes "${dependency_hash}")
                endif()
            endforeach()
        endif()
    endforeach()
endforeach()
string(SHA256 QUICKHEAP_BENCH_DEPENDENCY_DIGEST "${dependency_hashes}")
configure_file(build_info.hpp.in build_info.hpp @ONLY)

add_library(quickheap_bench_backend STATIC backend.cpp experiment_simd.cpp simd.cpp)
target_link_libraries(quickheap_bench_backend PUBLIC highway::hwy)
target_include_directories(quickheap_bench_backend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(QUICKHEAP_SCALAR)
    target_compile_definitions(quickheap_bench_backend PRIVATE HWY_COMPILE_ONLY_SCALAR=1)
else()
    target_compile_definitions(quickheap_bench_backend PRIVATE HWY_COMPILE_ALL_ATTAINABLE=1)
endif()
add_executable(quickheap_experiments experiments.cpp)
target_include_directories(quickheap_experiments PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(quickheap_experiments PRIVATE quickheap::quickheap quickheap_bench_backend)
target_link_libraries(quickheap_bench PRIVATE quickheap_bench_backend)
target_link_libraries(quickheap_bench_graph PRIVATE quickheap_bench_backend)

if(BUILD_TESTING)
    find_package(Catch2 3.16 REQUIRED CONFIG)
    add_executable(quickheap_bench_tests graph_test.cpp workload_test.cpp experimental_heap_test.cpp competitor_test.cpp fixture_test.cpp direct_workload_test.cpp original_profile_test.cpp scalar_heap_test.cpp backend_test.cpp diagnostics_test.cpp)
    target_link_libraries(quickheap_bench_tests PRIVATE quickheap_bench_graph_support Catch2::Catch2WithMain)
    target_link_libraries(quickheap_bench_tests PRIVATE quickheap_bench_backend)
endif()

add_subdirectory(external)
if(TARGET quickheap_bench_external)
    target_link_libraries(quickheap_bench PRIVATE quickheap_bench_external)
    target_link_libraries(quickheap_bench_graph PRIVATE quickheap_bench_external)
    if(BUILD_TESTING)
        target_sources(quickheap_bench_tests PRIVATE external_test.cpp radix_external_test.cpp)
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
    add_test(NAME quickheap_bench_contracts COMMAND quickheap_bench_tests)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME quickheap_report_arithmetic
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/evals/arithmetic_test.py)
    add_test(NAME quickheap_report_contract
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/evals/report_test.py)
    add_test(NAME quickheap_fixture_inspect
        COMMAND quickheap_fixture --inspect ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/HeapSort-i32-n8.qhf --dump)
    add_test(NAME quickheap_fixture_correctness
        COMMAND quickheap_bench --fixture ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/MonotoneConstantSize-i64-n8.qhf
            --heap std_priority_queue --verify-only)
    add_test(NAME quickheap_fixture_timing
        COMMAND quickheap_bench --fixture ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/MonotoneWiggle-i32-n8.qhf
            --heap std_priority_queue --repeat 1 --warmup 0)
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

if(QUICKHEAP_BENCH_FIXED_TARGETS)
    foreach(backend scalar avx2 avx512)
        add_executable(quickheap_bench_${backend} bench.cpp)
        target_include_directories(quickheap_bench_${backend} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
        target_compile_definitions(quickheap_bench_${backend} PRIVATE QUICKHEAP_BENCH_FIXED_BACKEND="${backend}")
        target_link_libraries(quickheap_bench_${backend} PRIVATE quickheap::quickheap quickheap_bench_backend)
        if(TARGET quickheap_bench_external)
            target_link_libraries(quickheap_bench_${backend} PRIVATE quickheap_bench_external)
        endif()
        if(QUICKHEAP_BENCH_BOOST)
            target_link_libraries(quickheap_bench_${backend} PRIVATE Boost::headers)
            target_compile_definitions(quickheap_bench_${backend} PRIVATE QUICKHEAP_BENCH_BOOST=1)
        endif()
        install(TARGETS quickheap_bench_${backend} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endforeach()
endif()

install(TARGETS quickheap_bench quickheap_bench_graph quickheap_experiments quickheap_fixture
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS run-all.sh DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench)
install(DIRECTORY data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench/data
    FILES_MATCHING PATTERN "*.gr")
install(DIRECTORY evals/ DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench/evals
    FILES_MATCHING PATTERN "*.py" PATTERN "historical.json" PATTERN "README.md")
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench)
install(FILES protocol.json catalog.csv DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench)
install(FILES PROTOCOL.md COVERAGE.md verification.md DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench)
install(PROGRAMS verify.py verify_pair.py verify_scalar.py verify_graph.py DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench)
install(DIRECTORY fixtures/ DESTINATION ${CMAKE_INSTALL_DATADIR}/quickheap-bench/fixtures
    FILES_MATCHING PATTERN "*.qhf")
