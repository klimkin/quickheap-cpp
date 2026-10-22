include(FetchContent)

option(QUICKHEAP_BENCH_EXTERNAL "Build original upstream benchmark competitors" OFF)
if(NOT QUICKHEAP_BENCH_EXTERNAL)
    return()
endif()

# Pin the benchmark's original gitlinks. Do not configure upstream projects.
function(quickheap_fetch_external name repository revision sha256)
    FetchContent_Declare(${name}
        URL "${repository}/archive/${revision}.tar.gz"
        URL_HASH "SHA256=${sha256}"
        SOURCE_SUBDIR quickheap_unused_cmake_directory)
    FetchContent_MakeAvailable(${name})
    FetchContent_GetProperties(${name} SOURCE_DIR source_dir)
    set(${name}_SOURCE_DIR "${source_dir}" PARENT_SCOPE)
endfunction()

quickheap_fetch_external(sequence_heap
    https://github.com/raphinesse/SequenceHeap
    ddbf7507b9ddb74a80cbca368575b8a052a90bc3
    a1d5873f24dc4c4685ea0bd0eff1daccf6796be6b24810148a8e46b380319b88)
quickheap_fetch_external(s3q
    https://github.com/raphinesse/s3q
    3621d1e79408164d942e9169a1ef4c5f6d695c52
    3d681a1629a78e88edc334cff960cef1a64c0294b6b1dd187e1d10ebdcf2ebd3)
quickheap_fetch_external(ips4o
    https://github.com/raphinesse/ips4o
    49c6fb294b829e171d85a4ffe7c279904fc41d4c
    df88f0fb3ecef1350635dd2d1f3c41f848a74503a2808df96fea07d84b03f95e)
quickheap_fetch_external(range_v3
    https://github.com/ericniebler/range-v3
    a81477931a8aa2ad025c6bda0609f38e09e4d7ec
    612b5d89f58a578240b28a1304ffb0d085686ebe0137adf175ed0e3382b7ed58)
quickheap_fetch_external(xoshiro
    https://github.com/Reputeless/Xoshiro-cpp
    078bf3396c31c3fa6d6f6c785b4612fb547f7132
    6d71c238c4b37778c7903b4ad2a4d61cd32db373e13ec9b310ed48dc91ae0362)
quickheap_fetch_external(original_quickheap
    https://github.com/RagnarGrootKoerkamp/quickheap
    a9f2518d9ed452130361cfe1681c68625a68ab35
    2a12c2e2c20cfdfdb4d501935c816f951b29416eb45a516f1546f96de3701da0)

set(QUICKHEAP_SEQUENCE_SOURCE_DIR "${sequence_heap_SOURCE_DIR}")
include(patch_sequence_heap.cmake)

add_library(quickheap_bench_external STATIC ../external_heaps.cpp)
set_target_properties(quickheap_bench_external PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_compile_features(quickheap_bench_external PUBLIC cxx_std_17)
target_compile_definitions(quickheap_bench_external PUBLIC QUICKHEAP_BENCH_EXTERNAL=1)
target_include_directories(quickheap_bench_external PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_include_directories(quickheap_bench_external SYSTEM PRIVATE
    "${sequence_heap_SOURCE_DIR}"
    "${s3q_SOURCE_DIR}/include"
    "${ips4o_SOURCE_DIR}/include"
    "${range_v3_SOURCE_DIR}/include"
    "${xoshiro_SOURCE_DIR}"
    "${original_quickheap_SOURCE_DIR}/quickheap-dev/ext/original-quickheap-sys/csrc")
find_package(Threads REQUIRED)
target_link_libraries(quickheap_bench_external PRIVATE Threads::Threads)
