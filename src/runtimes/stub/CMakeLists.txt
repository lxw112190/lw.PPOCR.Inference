add_library(lw_ppocr_runtime_stub SHARED
    stub_runtime.cpp
)
add_library(lw_ppocr_runtime_stub_forward SHARED
    stub_runtime.cpp
)
target_include_directories(lw_ppocr_runtime_stub
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime/include
)
target_include_directories(lw_ppocr_runtime_stub_forward
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime/include
)
target_compile_definitions(lw_ppocr_runtime_stub
    PRIVATE LW_PPOCR_RUNTIME_BUILDING_LIBRARY WIN32_LEAN_AND_MEAN NOMINMAX
)
target_compile_definitions(lw_ppocr_runtime_stub_forward
    PRIVATE
        LW_PPOCR_RUNTIME_BUILDING_LIBRARY
        LW_PPOCR_STUB_FORWARD_COMPAT
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)
set_target_properties(lw_ppocr_runtime_stub PROPERTIES
    OUTPUT_NAME "lw.PPOCR.Runtime.Stub"
)
set_target_properties(lw_ppocr_runtime_stub_forward PROPERTIES
    OUTPUT_NAME "lw.PPOCR.Runtime.Stub.Forward"
)

if(MSVC)
    target_compile_options(lw_ppocr_runtime_stub PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(lw_ppocr_runtime_stub_forward PRIVATE /W4 /permissive- /utf-8)
endif()
