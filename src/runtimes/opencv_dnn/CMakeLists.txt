set(LW_PPOCR_OPENCV_RUNTIME_SOURCES opencv_engine.cpp runtime_entry.cpp)
if(WIN32)
    list(APPEND LW_PPOCR_OPENCV_RUNTIME_SOURCES version.rc)
endif()
add_library(lw_ppocr_runtime_opencv_dnn SHARED
    ${LW_PPOCR_OPENCV_RUNTIME_SOURCES}
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
if(WIN32)
    set(LW_PPOCR_OPENCV_INSTALL_DIR "runtimes/win-x64/opencv")
else()
    set(LW_PPOCR_OPENCV_INSTALL_DIR "runtimes/linux-x64/opencv")
    set_target_properties(lw_ppocr_runtime_opencv_dnn PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()
install(TARGETS lw_ppocr_runtime_opencv_dnn
    RUNTIME DESTINATION "${LW_PPOCR_OPENCV_INSTALL_DIR}"
    LIBRARY DESTINATION "${LW_PPOCR_OPENCV_INSTALL_DIR}"
)

if(MSVC)
    target_compile_options(lw_ppocr_runtime_opencv_dnn
        PRIVATE /W4 /permissive- /utf-8 /fp:fast
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lw_ppocr_runtime_opencv_dnn PRIVATE
        -Wall -Wextra -Wpedantic -ffast-math
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

if(NOT WIN32)
    foreach(LW_PPOCR_OPENCV_MODULE core imgproc imgcodecs dnn)
        if(TARGET opencv_${LW_PPOCR_OPENCV_MODULE})
            # FOLLOW_SYMLINK_CHAIN belongs to file(INSTALL), not install(FILES).
            # Start at the SONAME link so the package receives both the link and
            # its versioned library while still honoring --prefix and DESTDIR.
            install(CODE "
                file(INSTALL
                    \"$<TARGET_SONAME_FILE:opencv_${LW_PPOCR_OPENCV_MODULE}>\"
                    DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${LW_PPOCR_OPENCV_INSTALL_DIR}\"
                    FOLLOW_SYMLINK_CHAIN
                )
            ")
        endif()
    endforeach()
endif()
