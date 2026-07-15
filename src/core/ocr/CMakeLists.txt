add_library(lw_ppocr_ocr_core STATIC
    image_ops.cpp
    db_postprocessor.cpp
    model_manifest.cpp
    ${CMAKE_SOURCE_DIR}/third_party/clipper/clipper.cpp
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
target_compile_definitions(lw_ppocr_ocr_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    target_compile_options(lw_ppocr_ocr_core PRIVATE /W4 /permissive- /utf-8)
endif()
