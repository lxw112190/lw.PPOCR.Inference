if(NOT TARGET openvino::runtime)
    message(FATAL_ERROR "OpenVINO Runtime CMake target was not found")
endif()

set(LW_PPOCR_OPENVINO_SOURCES
    openvino_session.cpp
    openvino_engine.cpp
    runtime_entry.cpp
)
if(WIN32)
    list(APPEND LW_PPOCR_OPENVINO_SOURCES version.rc)
endif()

add_library(lw_ppocr_runtime_openvino SHARED ${LW_PPOCR_OPENVINO_SOURCES})
target_include_directories(lw_ppocr_runtime_openvino
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(lw_ppocr_runtime_openvino
    PRIVATE lw_ppocr_ocr_core openvino::runtime ${OpenCV_LIBS}
)
target_compile_definitions(lw_ppocr_runtime_openvino
    PRIVATE LW_PPOCR_RUNTIME_BUILDING_LIBRARY WIN32_LEAN_AND_MEAN NOMINMAX
)
set_target_properties(lw_ppocr_runtime_openvino PROPERTIES
    OUTPUT_NAME "lw.PPOCR.Runtime.OpenVINO"
)

if(WIN32)
    set(LW_PPOCR_OPENVINO_INSTALL_DIR "runtimes/win-x64/openvino")
    set(LW_PPOCR_OPENVINO_BIN
        "${LW_PPOCR_OPENVINO_ROOT}/runtime/bin/intel64/Release")
    set(LW_PPOCR_OPENVINO_TBB_BIN
        "${LW_PPOCR_OPENVINO_ROOT}/runtime/3rdparty/tbb/bin")
else()
    set(LW_PPOCR_OPENVINO_INSTALL_DIR "runtimes/linux-x64/openvino")
    set(LW_PPOCR_OPENVINO_LIB
        "${LW_PPOCR_OPENVINO_ROOT}/runtime/lib/intel64")
    set(LW_PPOCR_OPENVINO_TBB_LIB
        "${LW_PPOCR_OPENVINO_ROOT}/runtime/3rdparty/tbb/lib")
    if(NOT EXISTS "${LW_PPOCR_OPENVINO_LIB}/libopenvino.so")
        message(FATAL_ERROR
            "OpenVINO Linux Runtime is incomplete: ${LW_PPOCR_OPENVINO_ROOT}")
    endif()
    set_target_properties(lw_ppocr_runtime_openvino PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

install(TARGETS lw_ppocr_runtime_openvino
    RUNTIME DESTINATION "${LW_PPOCR_OPENVINO_INSTALL_DIR}"
    LIBRARY DESTINATION "${LW_PPOCR_OPENVINO_INSTALL_DIR}"
)

if(WIN32)
    set(LW_PPOCR_OPENVINO_RUNTIME_DLLS
        openvino.dll
        openvino_onnx_frontend.dll
        openvino_intel_cpu_plugin.dll
    )
    foreach(runtime_dll IN LISTS LW_PPOCR_OPENVINO_RUNTIME_DLLS)
        if(NOT EXISTS "${LW_PPOCR_OPENVINO_BIN}/${runtime_dll}")
            message(FATAL_ERROR "OpenVINO dependency is missing: ${runtime_dll}")
        endif()
        add_custom_command(TARGET lw_ppocr_runtime_openvino POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LW_PPOCR_OPENVINO_BIN}/${runtime_dll}"
                $<TARGET_FILE_DIR:lw_ppocr_runtime_openvino>
        )
        install(FILES "${LW_PPOCR_OPENVINO_BIN}/${runtime_dll}"
            DESTINATION "${LW_PPOCR_OPENVINO_INSTALL_DIR}"
        )
    endforeach()

    foreach(runtime_dll tbb12.dll tbbmalloc.dll)
        if(EXISTS "${LW_PPOCR_OPENVINO_TBB_BIN}/${runtime_dll}")
            install(FILES "${LW_PPOCR_OPENVINO_TBB_BIN}/${runtime_dll}"
                DESTINATION "${LW_PPOCR_OPENVINO_INSTALL_DIR}"
            )
            add_custom_command(TARGET lw_ppocr_runtime_openvino POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${LW_PPOCR_OPENVINO_TBB_BIN}/${runtime_dll}"
                    $<TARGET_FILE_DIR:lw_ppocr_runtime_openvino>
            )
        endif()
    endforeach()
else()
    # Keep the official Linux Runtime layout self-contained.  The CPU plugin
    # and ONNX frontend are loaded dynamically, so they must be distributed in
    # addition to the library linked by this Runtime.
    install(DIRECTORY "${LW_PPOCR_OPENVINO_LIB}/"
        DESTINATION "${LW_PPOCR_OPENVINO_INSTALL_DIR}"
        FILES_MATCHING
            PATTERN "libopenvino.so*"
            PATTERN "libopenvino_onnx_frontend.so*"
            PATTERN "libopenvino_intel_cpu_plugin.so*"
            PATTERN "plugins.xml"
    )
    if(EXISTS "${LW_PPOCR_OPENVINO_TBB_LIB}")
        install(DIRECTORY "${LW_PPOCR_OPENVINO_TBB_LIB}/"
            DESTINATION "${LW_PPOCR_OPENVINO_INSTALL_DIR}"
            FILES_MATCHING PATTERN "*.so*"
        )
    endif()

    # OCR preprocessing/postprocessing and HTTP image decoding use OpenCV.
    # Install the shared modules beside the backend and use $ORIGIN RPATH.
    set(LW_PPOCR_OPENVINO_OPENCV_TARGETS ${OpenCV_LIBS})
    list(REMOVE_DUPLICATES LW_PPOCR_OPENVINO_OPENCV_TARGETS)
    foreach(LW_PPOCR_OPENCV_TARGET IN LISTS LW_PPOCR_OPENVINO_OPENCV_TARGETS)
        if(TARGET ${LW_PPOCR_OPENCV_TARGET})
            get_target_property(LW_PPOCR_OPENCV_TARGET_TYPE
                ${LW_PPOCR_OPENCV_TARGET} TYPE)
        else()
            set(LW_PPOCR_OPENCV_TARGET_TYPE "")
        endif()
        if(LW_PPOCR_OPENCV_TARGET_TYPE STREQUAL "SHARED_LIBRARY")
            install(CODE "
                file(INSTALL
                    \"$<TARGET_SONAME_FILE:${LW_PPOCR_OPENCV_TARGET}>\"
                    DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${LW_PPOCR_OPENVINO_INSTALL_DIR}\"
                    FOLLOW_SYMLINK_CHAIN
                )
            ")
        endif()
    endforeach()
endif()

if(EXISTS "${LW_PPOCR_OPENVINO_ROOT}/docs/licensing")
    install(DIRECTORY "${LW_PPOCR_OPENVINO_ROOT}/docs/licensing/"
        DESTINATION licenses/openvino-package)
endif()

if(TARGET opencv_world)
    install(FILES $<TARGET_FILE:opencv_world>
        DESTINATION "${LW_PPOCR_OPENVINO_INSTALL_DIR}"
    )
    if(WIN32)
        add_custom_command(TARGET lw_ppocr_runtime_openvino POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:opencv_world>
                $<TARGET_FILE_DIR:lw_ppocr_runtime_openvino>
        )
    endif()
endif()

if(MSVC)
    target_compile_options(lw_ppocr_runtime_openvino
        PRIVATE /W4 /permissive- /utf-8 /fp:fast
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lw_ppocr_runtime_openvino PRIVATE
        -Wall -Wextra -Wpedantic -ffast-math
    )
endif()
