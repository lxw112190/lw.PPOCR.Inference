add_library(lw_ppocr_runtime_opencv_dnn SHARED
    opencv_engine.cpp
    runtime_entry.cpp
    version.rc
)
target_include_directories(lw_ppocr_runtime_opencv_dnn
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(lw_ppocr_runtime_opencv_dnn
    PRIVATE lw_ppocr_ocr_core ${OpenCV_LIBS}
)
target_compile_definitions(lw_ppocr_runtime_opencv_dnn
    PRIVATE LW_PPOCR_RUNTIME_BUILDING_LIBRARY WIN32_LEAN_AND_MEAN NOMINMAX
)
set_target_properties(lw_ppocr_runtime_opencv_dnn PROPERTIES
    OUTPUT_NAME "lw.PPOCR.Runtime.OpenCVDNN"
)
set(LW_PPOCR_OPENCV_INSTALL_DIR "runtimes/win-x64/opencv")
install(TARGETS lw_ppocr_runtime_opencv_dnn
    RUNTIME DESTINATION "${LW_PPOCR_OPENCV_INSTALL_DIR}"
)

if(MSVC)
    target_compile_options(lw_ppocr_runtime_opencv_dnn
        PRIVATE /W4 /permissive- /utf-8 /fp:fast
    )
endif()

if(TARGET opencv_world)
    install(FILES $<TARGET_FILE:opencv_world>
        DESTINATION "${LW_PPOCR_OPENCV_INSTALL_DIR}"
    )
    add_custom_command(TARGET lw_ppocr_runtime_opencv_dnn POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:opencv_world>
            $<TARGET_FILE_DIR:lw_ppocr_runtime_opencv_dnn>
    )
endif()
