set(LW_PPOCR_OPENVINO_BIN "${LW_PPOCR_OPENVINO_ROOT}/runtime/bin/intel64/Release")
set(LW_PPOCR_OPENVINO_TBB_BIN "${LW_PPOCR_OPENVINO_ROOT}/runtime/3rdparty/tbb/bin")

if(NOT TARGET openvino::runtime)
    message(FATAL_ERROR "OpenVINO Runtime CMake target was not found")
endif()

add_library(lw_ppocr_runtime_openvino SHARED
    openvino_session.cpp
    openvino_engine.cpp
    runtime_entry.cpp
    version.rc
)
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
set(LW_PPOCR_OPENVINO_INSTALL_DIR "runtimes/win-x64/openvino")
install(TARGETS lw_ppocr_runtime_openvino
    RUNTIME DESTINATION "${LW_PPOCR_OPENVINO_INSTALL_DIR}"
)

if(MSVC)
    target_compile_options(lw_ppocr_runtime_openvino
        PRIVATE /W4 /permissive- /utf-8 /fp:fast
    )
endif()

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
endforeach()
foreach(runtime_dll IN LISTS LW_PPOCR_OPENVINO_RUNTIME_DLLS)
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

if(TARGET opencv_world)
    install(FILES $<TARGET_FILE:opencv_world>
        DESTINATION "${LW_PPOCR_OPENVINO_INSTALL_DIR}"
    )
    add_custom_command(TARGET lw_ppocr_runtime_openvino POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:opencv_world>
            $<TARGET_FILE_DIR:lw_ppocr_runtime_openvino>
    )
endif()
