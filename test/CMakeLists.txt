find_package(GTest REQUIRED CONFIG)
add_executable(quickheap_tests quickheap_test.cpp c_api_test.c)
target_link_libraries(quickheap_tests PRIVATE quickheap GTest::gtest_main)
include(GoogleTest)
gtest_add_tests(TARGET quickheap_tests SOURCES quickheap_test.cpp)

add_executable(quickheap_trace trace.cpp)
target_link_libraries(quickheap_trace PRIVATE quickheap)

if(QUICKHEAP_BUILD_BENCHMARKS)
    add_subdirectory(${PROJECT_SOURCE_DIR}/benchmark ${PROJECT_BINARY_DIR}/benchmark)
endif()
