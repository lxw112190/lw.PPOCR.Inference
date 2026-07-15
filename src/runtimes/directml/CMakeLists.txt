set(LW_PPOCR_ORT_INCLUDE "${LW_PPOCR_ONNXRUNTIME_DML_ROOT}/include")
set(LW_PPOCR_ORT_LIB_DIR "${LW_PPOCR_ONNXRUNTIME_DML_ROOT}/lib")

if(NOT EXISTS "${LW_PPOCR_ORT_INCLUDE}/onnxruntime_cxx_api.h" OR
   NOT EXISTS "${LW_PPOCR_ORT_LIB_DIR}/onnxruntime.lib")
    message(FATAL_ERROR "The ONNX Runtime DirectML package is incomplete")
endif()
if(NOT LW_PPOCR_DIRECTML_DLL OR NOT EXISTS "${LW_PPOCR_DIRECTML_DLL}")
    message(FATAL_ERROR "LW_PPOCR_DIRECTML_DLL must point to the DirectML.dll shipped with the package")
endif()

add_library(lw_ppocr_runtime_directml SHARED
    ort_session.cpp
    directml_engine.cpp
    runtime_entry.cpp
    version.rc
)
target_include_directories(lw_ppocr_runtime_directml
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LW_PPOCR_ORT_INCLUDE}
        ${OpenCV_INCLUDE_DIRS}
)
target_link_directories(lw_ppocr_runtime_directml PRIVATE ${LW_PPOCR_ORT_LIB_DIR})
target_link_libraries(lw_ppocr_runtime_directml
    PRIVATE lw_ppocr_ocr_core onnxruntime ${OpenCV_LIBS}
)
target_compile_definitions(lw_ppocr_runtime_directml
    PRIVATE LW_PPOCR_RUNTIME_BUILDING_LIBRARY WIN32_LEAN_AND_MEAN NOMINMAX
)
set_target_properties(lw_ppocr_runtime_directml PROPERTIES
    OUTPUT_NAME "lw.PPOCR.Runtime.DirectML"
)
set(LW_PPOCR_DIRECTML_INSTALL_DIR "runtimes/win-x64/directml")
install(TARGETS lw_ppocr_runtime_directml
    RUNTIME DESTINATION "${LW_PPOCR_DIRECTML_INSTALL_DIR}"
)
install(FILES
    "${LW_PPOCR_ORT_LIB_DIR}/onnxruntime.dll"
    "${LW_PPOCR_DIRECTML_DLL}"
    DESTINATION "${LW_PPOCR_DIRECTML_INSTALL_DIR}"
)
if(EXISTS "${LW_PPOCR_ORT_LIB_DIR}/onnxruntime_providers_shared.dll")
    install(FILES "${LW_PPOCR_ORT_LIB_DIR}/onnxruntime_providers_shared.dll"
        DESTINATION "${LW_PPOCR_DIRECTML_INSTALL_DIR}"
    )
endif()

if(MSVC)
    target_compile_options(lw_ppocr_runtime_directml
        PRIVATE /W4 /permissive- /utf-8 /fp:fast
    )
endif()

add_custom_command(TARGET lw_ppocr_runtime_directml POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LW_PPOCR_ORT_LIB_DIR}/onnxruntime.dll"
        $<TARGET_FILE_DIR:lw_ppocr_runtime_directml>
)
add_custom_command(TARGET lw_ppocr_runtime_directml POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LW_PPOCR_DIRECTML_DLL}"
        $<TARGET_FILE_DIR:lw_ppocr_runtime_directml>
)
if(EXISTS "${LW_PPOCR_ORT_LIB_DIR}/onnxruntime_providers_shared.dll")
    add_custom_command(TARGET lw_ppocr_runtime_directml POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LW_PPOCR_ORT_LIB_DIR}/onnxruntime_providers_shared.dll"
            $<TARGET_FILE_DIR:lw_ppocr_runtime_directml>
    )
endif()
if(TARGET opencv_world)
    install(FILES $<TARGET_FILE:opencv_world>
        DESTINATION "${LW_PPOCR_DIRECTML_INSTALL_DIR}"
    )
    add_custom_command(TARGET lw_ppocr_runtime_directml POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:opencv_world>
            $<TARGET_FILE_DIR:lw_ppocr_runtime_directml>
    )
endif()
