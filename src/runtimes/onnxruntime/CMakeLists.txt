set(LW_PPOCR_ORT_INCLUDE "${LW_PPOCR_ONNXRUNTIME_ROOT}/include")
set(LW_PPOCR_ORT_LIB_DIR "${LW_PPOCR_ONNXRUNTIME_ROOT}/lib")

if(NOT EXISTS "${LW_PPOCR_ORT_INCLUDE}/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "The ONNX Runtime package has no C/C++ headers")
endif()
if(WIN32)
    set(LW_PPOCR_ORT_LINK_LIBRARY "${LW_PPOCR_ORT_LIB_DIR}/onnxruntime.lib")
    set(LW_PPOCR_ORT_SHARED_LIBRARY "${LW_PPOCR_ORT_LIB_DIR}/onnxruntime.dll")
    set(LW_PPOCR_ORT_INSTALL_DIR "runtimes/win-x64/onnxruntime")
else()
    set(LW_PPOCR_ORT_LINK_LIBRARY "${LW_PPOCR_ORT_LIB_DIR}/libonnxruntime.so")
    set(LW_PPOCR_ORT_SHARED_LIBRARY "${LW_PPOCR_ORT_LIB_DIR}/libonnxruntime.so")
    set(LW_PPOCR_ORT_INSTALL_DIR "runtimes/linux-x64/onnxruntime")
endif()
if(NOT EXISTS "${LW_PPOCR_ORT_LINK_LIBRARY}" OR
   NOT EXISTS "${LW_PPOCR_ORT_SHARED_LIBRARY}")
    message(FATAL_ERROR "The ONNX Runtime package is incomplete: ${LW_PPOCR_ONNXRUNTIME_ROOT}")
endif()

add_library(lw_ppocr_runtime_onnxruntime SHARED
    onnxruntime_engine.cpp
    ort_session.cpp
    runtime_entry.cpp
)
# MSVC's include scanner does not always track an included .cpp file. Declare
# these dependencies explicitly so edits to the shared implementation rebuild
# the portable specialization during incremental development.
set_source_files_properties(onnxruntime_engine.cpp PROPERTIES
    OBJECT_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/runtimes/directml/directml_engine.cpp;${CMAKE_SOURCE_DIR}/src/runtimes/directml/directml_engine.hpp"
)
set_source_files_properties(ort_session.cpp PROPERTIES
    OBJECT_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/runtimes/directml/ort_session.cpp;${CMAKE_SOURCE_DIR}/src/runtimes/directml/ort_session.hpp"
)
set_source_files_properties(runtime_entry.cpp PROPERTIES
    OBJECT_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/runtimes/directml/runtime_entry.cpp"
)
target_include_directories(lw_ppocr_runtime_onnxruntime
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime/include
        ${CMAKE_SOURCE_DIR}/src/runtimes/directml
        ${LW_PPOCR_ORT_INCLUDE}
        ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(lw_ppocr_runtime_onnxruntime
    PRIVATE lw_ppocr_ocr_core "${LW_PPOCR_ORT_LINK_LIBRARY}" ${OpenCV_LIBS}
)
target_compile_definitions(lw_ppocr_runtime_onnxruntime
    PRIVATE LW_PPOCR_RUNTIME_BUILDING_LIBRARY WIN32_LEAN_AND_MEAN NOMINMAX
)
set_target_properties(lw_ppocr_runtime_onnxruntime PROPERTIES
    OUTPUT_NAME "lw.PPOCR.Runtime.ONNXRuntime"
)
if(NOT WIN32)
    set_target_properties(lw_ppocr_runtime_onnxruntime PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()
install(TARGETS lw_ppocr_runtime_onnxruntime
    RUNTIME DESTINATION "${LW_PPOCR_ORT_INSTALL_DIR}"
    LIBRARY DESTINATION "${LW_PPOCR_ORT_INSTALL_DIR}"
)
if(EXISTS "${LW_PPOCR_ONNXRUNTIME_ROOT}/LICENSE")
    install(FILES "${LW_PPOCR_ONNXRUNTIME_ROOT}/LICENSE"
        DESTINATION licenses
        RENAME onnxruntime-package-MIT.txt
    )
endif()
if(EXISTS "${LW_PPOCR_ONNXRUNTIME_ROOT}/ThirdPartyNotices.txt")
    install(FILES "${LW_PPOCR_ONNXRUNTIME_ROOT}/ThirdPartyNotices.txt"
        DESTINATION licenses
        RENAME onnxruntime-package-ThirdPartyNotices.txt
    )
endif()

if(WIN32)
    install(FILES "${LW_PPOCR_ORT_SHARED_LIBRARY}"
        DESTINATION "${LW_PPOCR_ORT_INSTALL_DIR}"
    )
    add_custom_command(TARGET lw_ppocr_runtime_onnxruntime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LW_PPOCR_ORT_SHARED_LIBRARY}"
            $<TARGET_FILE_DIR:lw_ppocr_runtime_onnxruntime>
    )
else()
    # Preserve libonnxruntime.so -> SONAME -> versioned-file links.
    install(CODE "
        file(INSTALL
            \"${LW_PPOCR_ORT_SHARED_LIBRARY}\"
            DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${LW_PPOCR_ORT_INSTALL_DIR}\"
            FOLLOW_SYMLINK_CHAIN
        )
    ")

    # The shared OCR preprocessing/postprocessing layer uses OpenCV. Keep the
    # required modules beside this Runtime so its RPATH remains self-contained.
    set(LW_PPOCR_ORT_OPENCV_TARGETS ${OpenCV_LIBS})
    list(REMOVE_DUPLICATES LW_PPOCR_ORT_OPENCV_TARGETS)
    foreach(LW_PPOCR_OPENCV_TARGET IN LISTS LW_PPOCR_ORT_OPENCV_TARGETS)
        if(TARGET ${LW_PPOCR_OPENCV_TARGET})
            get_target_property(LW_PPOCR_OPENCV_TARGET_TYPE
                ${LW_PPOCR_OPENCV_TARGET} TYPE
            )
        else()
            set(LW_PPOCR_OPENCV_TARGET_TYPE "")
        endif()
        if(LW_PPOCR_OPENCV_TARGET_TYPE STREQUAL "SHARED_LIBRARY")
            install(CODE "
                file(INSTALL
                    \"$<TARGET_SONAME_FILE:${LW_PPOCR_OPENCV_TARGET}>\"
                    DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${LW_PPOCR_ORT_INSTALL_DIR}\"
                    FOLLOW_SYMLINK_CHAIN
                )
            ")
        endif()
    endforeach()
endif()

if(MSVC)
    target_compile_options(lw_ppocr_runtime_onnxruntime
        PRIVATE /W4 /permissive- /utf-8 /fp:fast
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lw_ppocr_runtime_onnxruntime PRIVATE
        -Wall -Wextra -Wpedantic -ffast-math
    )
endif()
