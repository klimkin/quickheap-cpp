find_package(Catch2 3.16 REQUIRED CONFIG)
add_executable(quickheap_tests quickheap_test.cpp c_api_test.c)
target_link_libraries(quickheap_tests PRIVATE quickheap Catch2::Catch2)
include(Catch)
catch_discover_tests(quickheap_tests)

add_executable(quickheap_allocation_retry allocation_retry_test.cpp)
target_link_libraries(quickheap_allocation_retry PRIVATE quickheap)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_FLAGS MATCHES "-fsanitize=[^ ]*thread")
    target_link_options(quickheap_allocation_retry PRIVATE -fno-sanitize-link-c++-runtime)
endif()
add_test(NAME quickheap_allocation_retry COMMAND quickheap_allocation_retry)

add_executable(quickheap_benchmark quickheap_benchmark.cpp)
target_link_libraries(quickheap_benchmark PRIVATE quickheap Catch2::Catch2WithMain)

add_executable(quickheap_trace trace.cpp)
target_link_libraries(quickheap_trace PRIVATE quickheap)
