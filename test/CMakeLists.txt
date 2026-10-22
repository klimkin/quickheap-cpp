find_package(Catch2 3.16.0 EXACT REQUIRED CONFIG)
add_executable(quickheap_tests quickheap_test.cpp c_api_test.c)
target_link_libraries(quickheap_tests PRIVATE quickheap Catch2::Catch2)
include(Catch)
catch_discover_tests(quickheap_tests)

add_executable(quickheap_benchmark quickheap_benchmark.cpp)
target_link_libraries(quickheap_benchmark PRIVATE quickheap Catch2::Catch2WithMain)

add_executable(quickheap_trace trace.cpp)
target_link_libraries(quickheap_trace PRIVATE quickheap)
