add_library(quickheap quickheap.cpp simd.cpp)
add_library(quickheap::quickheap ALIAS quickheap)
set_target_properties(quickheap PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME quickheap
)
target_compile_features(quickheap PRIVATE cxx_std_20)
target_include_directories(quickheap
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(quickheap PRIVATE highway::hwy)
target_compile_definitions(quickheap PRIVATE QUICKHEAP_BUILDING_LIBRARY=1)
get_target_property(quickheap_type quickheap TYPE)
if(quickheap_type STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(quickheap PUBLIC QUICKHEAP_SHARED=1)
    set(QUICKHEAP_STATIC OFF PARENT_SCOPE)
else()
    set(QUICKHEAP_STATIC ON PARENT_SCOPE)
endif()
if(QUICKHEAP_SCALAR)
    target_compile_definitions(quickheap PRIVATE HWY_COMPILE_ONLY_SCALAR=1)
else()
    target_compile_definitions(quickheap PRIVATE HWY_COMPILE_ALL_ATTAINABLE=1)
endif()

install(TARGETS quickheap EXPORT quickheapTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    ${PROJECT_SOURCE_DIR}/include/quickheap.h
    ${PROJECT_SOURCE_DIR}/include/quickheap.hpp
    ${PROJECT_SOURCE_DIR}/include/key_value.hpp
    ${PROJECT_SOURCE_DIR}/include/export.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
