add_library(quickheap quickheap.cpp simd.cpp)
include(GenerateExportHeader)
generate_export_header(quickheap
    BASE_NAME QUICKHEAP
    EXPORT_MACRO_NAME QUICKHEAP_API
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/export.hpp"
)
target_include_directories(quickheap
    PUBLIC ${PROJECT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(quickheap PRIVATE highway::hwy)

get_target_property(quickheap_type quickheap TYPE)
if(quickheap_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(quickheap PUBLIC QUICKHEAP_STATIC_DEFINE)
endif()
if(QUICKHEAP_SCALAR)
    target_compile_definitions(quickheap PRIVATE HWY_COMPILE_ONLY_SCALAR=1)
else()
    target_compile_definitions(quickheap PRIVATE HWY_COMPILE_ALL_ATTAINABLE=1)
endif()

install(TARGETS quickheap
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    ${PROJECT_SOURCE_DIR}/include/quickheap.h
    ${PROJECT_SOURCE_DIR}/include/quickheap.hpp
    ${PROJECT_SOURCE_DIR}/include/key_value.hpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/export.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
