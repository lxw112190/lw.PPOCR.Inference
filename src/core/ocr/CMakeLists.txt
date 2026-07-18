add_library(lw_ppocr_ocr_core STATIC
    image_ops.cpp
    db_postprocessor.cpp
    model_manifest.cpp
    ${CMAKE_SOURCE_DIR}/third_party/clipper/clipper.cpp
)
set_target_properties(lw_ppocr_ocr_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(lw_ppocr_ocr_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/clipper
)
target_link_libraries(lw_ppocr_ocr_core PUBLIC ${OpenCV_LIBS})
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(lw_ppocr_ocr_core PUBLIC stdc++fs)
endif()
if(WIN32)
    target_compile_definitions(lw_ppocr_ocr_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(MSVC)
    target_compile_options(lw_ppocr_ocr_core PRIVATE /W4 /permissive- /utf-8)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lw_ppocr_ocr_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
