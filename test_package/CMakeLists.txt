cmake_minimum_required(VERSION 4.0)
project(quickheap_test_package LANGUAGES C CXX)

enable_testing()
find_package(quickheap REQUIRED CONFIG)

add_executable(quickheap_test_c test_c.c)
target_link_libraries(quickheap_test_c PRIVATE quickheap::quickheap)
set_target_properties(quickheap_test_c PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    LINKER_LANGUAGE CXX
)
add_test(NAME quickheap_test_c COMMAND quickheap_test_c)

add_executable(quickheap_test_cpp test_cpp.cpp)
target_link_libraries(quickheap_test_cpp PRIVATE quickheap::quickheap)
set_target_properties(quickheap_test_cpp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME quickheap_test_cpp COMMAND quickheap_test_cpp)
