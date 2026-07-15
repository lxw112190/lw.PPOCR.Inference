set(LW_PPOCR_TENSORRT_INCLUDE "${LW_PPOCR_TENSORRT_ROOT}/include")
set(LW_PPOCR_TENSORRT_LIB "${LW_PPOCR_TENSORRT_ROOT}/lib")
set(LW_PPOCR_TENSORRT_BIN "${LW_PPOCR_TENSORRT_ROOT}/bin")

foreach(required_file
    "${LW_PPOCR_TENSORRT_INCLUDE}/NvInfer.h"
    "${LW_PPOCR_TENSORRT_LIB}/nvinfer_10.lib"
    "${LW_PPOCR_TENSORRT_LIB}/nvinfer_plugin_10.lib"
    "${LW_PPOCR_TENSORRT_BIN}/nvinfer_10.dll"
    "${LW_PPOCR_TENSORRT_BIN}/nvinfer_plugin_10.dll")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "TensorRT dependency is missing: ${required_file}")
    endif()
endforeach()

add_library(lw_ppocr_runtime_tensorrt SHARED
    tensorrt_session.cpp
    tensorrt_engine.cpp
    runtime_entry.cpp
    version.rc
)
target_include_directories(lw_ppocr_runtime_tensorrt
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LW_PPOCR_TENSORRT_INCLUDE}
        ${OpenCV_INCLUDE_DIRS}
)
target_link_directories(lw_ppocr_runtime_tensorrt PRIVATE ${LW_PPOCR_TENSORRT_LIB})
target_link_libraries(lw_ppocr_runtime_tensorrt
    PRIVATE
        lw_ppocr_ocr_core
        nvinfer_10
        nvinfer_plugin_10
        CUDA::cudart
        ${OpenCV_LIBS}
)
target_compile_definitions(lw_ppocr_runtime_tensorrt
    PRIVATE LW_PPOCR_RUNTIME_BUILDING_LIBRARY WIN32_LEAN_AND_MEAN NOMINMAX
)
set_target_properties(lw_ppocr_runtime_tensorrt PROPERTIES
    OUTPUT_NAME "lw.PPOCR.Runtime.TensorRT"
)
set(LW_PPOCR_TENSORRT_INSTALL_DIR "runtimes/win-x64/tensorrt")
install(TARGETS lw_ppocr_runtime_tensorrt
    RUNTIME DESTINATION "${LW_PPOCR_TENSORRT_INSTALL_DIR}"
)
install(FILES
    "${LW_PPOCR_TENSORRT_BIN}/nvinfer_10.dll"
    "${LW_PPOCR_TENSORRT_BIN}/nvinfer_plugin_10.dll"
    "${CUDAToolkit_BIN_DIR}/cudart64_12.dll"
    DESTINATION "${LW_PPOCR_TENSORRT_INSTALL_DIR}"
)

if(MSVC)
    target_compile_options(lw_ppocr_runtime_tensorrt
        PRIVATE /W4 /permissive- /utf-8 /fp:fast
    )
endif()

foreach(runtime_dll nvinfer_10.dll nvinfer_plugin_10.dll)
    add_custom_command(TARGET lw_ppocr_runtime_tensorrt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LW_PPOCR_TENSORRT_BIN}/${runtime_dll}"
            $<TARGET_FILE_DIR:lw_ppocr_runtime_tensorrt>
    )
endforeach()
add_custom_command(TARGET lw_ppocr_runtime_tensorrt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CUDAToolkit_BIN_DIR}/cudart64_12.dll"
        $<TARGET_FILE_DIR:lw_ppocr_runtime_tensorrt>
)
if(TARGET opencv_world)
    install(FILES $<TARGET_FILE:opencv_world>
        DESTINATION "${LW_PPOCR_TENSORRT_INSTALL_DIR}"
    )
    add_custom_command(TARGET lw_ppocr_runtime_tensorrt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:opencv_world>
            $<TARGET_FILE_DIR:lw_ppocr_runtime_tensorrt>
    )
endif()
