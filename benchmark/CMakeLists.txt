add_executable(quickheap_benchmark quickheap_benchmark.cpp)
target_link_libraries(quickheap_benchmark PRIVATE quickheap Catch2::Catch2WithMain)
add_test(NAME quickheap_benchmark
    COMMAND quickheap_benchmark
        "[benchmark]"
        --section 1024
        --benchmark-samples 1
        --benchmark-warmup-time 0
        --benchmark-no-analysis)
