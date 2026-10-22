find_package(benchmark REQUIRED CONFIG)
add_executable(quickheap_benchmark quickheap_benchmark.cpp)
target_link_libraries(quickheap_benchmark PRIVATE quickheap benchmark::benchmark_main)
add_test(NAME quickheap_benchmark
    COMMAND quickheap_benchmark
        --benchmark_filter=/1024$
        --benchmark_min_time=0.001s
        --benchmark_repetitions=1)
set_tests_properties(quickheap_benchmark PROPERTIES
    FAIL_REGULAR_EXPRESSION "ERROR OCCURRED")
