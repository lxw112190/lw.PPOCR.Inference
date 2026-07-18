cmake_minimum_required(VERSION 3.24)

project(lw.PPOCR.Inference VERSION 1.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Keep the package layout stable while selecting the Linux Runtime directory
# from the target architecture. Cross-compilers may override
# CMAKE_SYSTEM_PROCESSOR; native x64 and ARM64 builds are detected directly.
if(WIN32)
    set(LW_PPOCR_PLATFORM_RID "win-x64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" LW_PPOCR_SYSTEM_PROCESSOR_LOWER)
    if(LW_PPOCR_SYSTEM_PROCESSOR_LOWER MATCHES "^(x86_64|amd64)$")
        set(LW_PPOCR_PLATFORM_RID "linux-x64")
    elseif(LW_PPOCR_SYSTEM_PROCESSOR_LOWER MATCHES "^(aarch64|arm64)$")
        set(LW_PPOCR_PLATFORM_RID "linux-arm64")
    else()
        message(FATAL_ERROR
            "Unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}. "
            "Supported architectures are x86_64 and aarch64.")
    endif()
else()
    set(LW_PPOCR_PLATFORM_RID "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(LW_PPOCR_RUNTIME_ROOT_RELATIVE "runtimes/${LW_PPOCR_PLATFORM_RID}")
message(STATUS "lw.PPOCR platform RID: ${LW_PPOCR_PLATFORM_RID}")

option(LW_PPOCR_BUILD_TESTS "Build ABI and loader smoke tests" ON)
option(LW_PPOCR_BUILD_STUB_RUNTIME "Build the contract-testing Stub Runtime" ON)
option(LW_PPOCR_BUILD_OPENCV_RUNTIME "Build the OpenCV DNN Runtime" OFF)
option(LW_PPOCR_BUILD_DIRECTML_RUNTIME "Build the ONNX Runtime DirectML Runtime" OFF)
option(LW_PPOCR_BUILD_ONNXRUNTIME_RUNTIME "Build the portable ONNX Runtime CPU/CUDA Runtime" OFF)
option(LW_PPOCR_BUILD_OPENVINO_RUNTIME "Build the OpenVINO Runtime" OFF)
option(LW_PPOCR_BUILD_TENSORRT_RUNTIME "Build the TensorRT Runtime" OFF)
option(LW_PPOCR_BUILD_HTTP_SERVICE "Build the cross-platform HTTP API service host" ON)
set(LW_PPOCR_OPENCV_ROOT "" CACHE PATH "OpenCV distribution root containing OpenCVConfig.cmake")
set(LW_PPOCR_ONNXRUNTIME_DML_ROOT "" CACHE PATH "ONNX Runtime DirectML package root")
set(LW_PPOCR_ONNXRUNTIME_ROOT "" CACHE PATH "Official ONNX Runtime CPU or GPU package root")
set(LW_PPOCR_DIRECTML_DLL "" CACHE FILEPATH "DirectML.dll distributed with ONNX Runtime DirectML")
set(LW_PPOCR_OPENVINO_ROOT "" CACHE PATH "OpenVINO toolkit root containing runtime/cmake/OpenVINOConfig.cmake")
set(LW_PPOCR_TENSORRT_ROOT "" CACHE PATH "TensorRT root containing include, lib, and bin")
set(LW_PPOCR_TEST_DET_MODEL "" CACHE FILEPATH "Detector ONNX used by the OpenCV integration test")
set(LW_PPOCR_TEST_REC_MODEL "" CACHE FILEPATH "Recognizer ONNX used by the OpenCV integration test")
set(LW_PPOCR_TEST_CLS_MODEL "" CACHE FILEPATH "Optional classifier ONNX used by integration tests")
set(LW_PPOCR_TEST_DICTIONARY "" CACHE FILEPATH "Dictionary used by the OpenCV integration test")
set(LW_PPOCR_TEST_IMAGE "" CACHE FILEPATH "Image used by the OpenCV integration test")
set(LW_PPOCR_TEST_DIRECTML_DEVICE_ID "0" CACHE STRING "DirectML adapter used by integration tests")
set(LW_PPOCR_TEST_OPENVINO_DEVICE "CPU" CACHE STRING "OpenVINO device used by integration tests")
set(LW_PPOCR_TEST_TENSORRT_DET_ENGINE "" CACHE FILEPATH "TensorRT detector engine used by integration tests")
set(LW_PPOCR_TEST_TENSORRT_REC_ENGINE "" CACHE FILEPATH "TensorRT recognizer engine used by integration tests")
set(LW_PPOCR_TEST_TENSORRT_CLS_ENGINE "" CACHE FILEPATH "Optional TensorRT classifier engine used by integration tests")
set(LW_PPOCR_TEST_TENSORRT_DICTIONARY "" CACHE FILEPATH "TensorRT recognition dictionary used by integration tests")
set(LW_PPOCR_TEST_TENSORRT_IMAGE "" CACHE FILEPATH "Image used by the TensorRT integration test")
set(LW_PPOCR_TEST_TENSORRT_DEVICE_ID "0" CACHE STRING "CUDA device used by integration tests")

add_library(lw_ppocr_core STATIC
    src/core/config_validation.cpp
)
set_target_properties(lw_ppocr_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(lw_ppocr_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
)
target_compile_definitions(lw_ppocr_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

set(LW_PPOCR_LOADER_SOURCES src/loader/loader.cpp)
if(WIN32)
    list(APPEND LW_PPOCR_LOADER_SOURCES src/loader/version.rc)
endif()
add_library(lw_ppocr_loader SHARED ${LW_PPOCR_LOADER_SOURCES})
target_include_directories(lw_ppocr_loader
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
)
target_link_libraries(lw_ppocr_loader PRIVATE lw_ppocr_core ${CMAKE_DL_LIBS})
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    # GCC 8 keeps C++17 filesystem symbols in a separate compatibility
    # library. Anolis OS 8.10 uses GCC 8 by default.
    target_link_libraries(lw_ppocr_loader PUBLIC stdc++fs)
endif()
target_compile_definitions(lw_ppocr_loader
    PRIVATE
        LW_PPOCR_BUILDING_LIBRARY WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
        LW_PPOCR_DEFAULT_RUNTIME_ROOT="${LW_PPOCR_RUNTIME_ROOT_RELATIVE}"
)
set_target_properties(lw_ppocr_loader PROPERTIES OUTPUT_NAME "lw.PPOCR")
if(NOT WIN32)
    set_target_properties(lw_ppocr_loader PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )
endif()

if(LW_PPOCR_BUILD_HTTP_SERVICE)
    add_subdirectory(src/http_service)
endif()

install(TARGETS lw_ppocr_loader
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
install(FILES README.md README_EN.md LICENSE DESTINATION .)
install(FILES third_party/clipper/LICENSE.txt
    DESTINATION licenses
    RENAME clipper-BSL-1.0.txt
)
install(DIRECTORY third_party/licenses/ DESTINATION licenses)
install(FILES third_party/cpp-httplib/LICENSE
    DESTINATION licenses
    RENAME cpp-httplib-MIT.txt
)
install(FILES third_party/nlohmann-json/LICENSE.MIT
    DESTINATION licenses
    RENAME nlohmann-json-MIT.txt
)
install(FILES
    docs/abi-freeze-candidate.md
    docs/abi-v1-stability.md
    docs/architecture.md
    docs/compatibility-matrix.md
    docs/license-audit.md
    docs/migration-roadmap.md
    docs/model-manifest.md
    docs/performance-report.md
    docs/releases/v1.0.0.md
    docs/releases/v1.1.0.md
    docs/releases/v1.3.0.md
    docs/releases/v0.9.0-rc.1.md
    docs/releases/v0.8.0.md
    docs/releases/v0.3.0.md
    docs/releases/v0.2.0.md
    docs/releases/v0.1.0.md
    docs/stability-testing.md
    docs/tensorrt-engines.md
    docs/http-service.md
    docs/linux-opencv.md
    docs/linux-opencv-openeuler-arm64.md
    docs/linux-opencv-domestic-arm64.md
    docs/linux-domestic-arm64-ci.md
    docs/linux-onnxruntime.md
    docs/linux-openvino.md
    models/model-manifest.schema.json
    DESTINATION docs
)
install(FILES models/model-manifest.schema.json DESTINATION models)
install(DIRECTORY models/example/ DESTINATION models/example)
install(DIRECTORY models/ppocrv6-tiny/ DESTINATION models/ppocrv6-tiny)
install(DIRECTORY examples/ DESTINATION examples
    PATTERN "*.pyc" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
)

if(MSVC)
    target_compile_options(lw_ppocr_core PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(lw_ppocr_loader PRIVATE /W4 /permissive- /utf-8)
endif()

if(LW_PPOCR_BUILD_STUB_RUNTIME)
    add_subdirectory(src/runtimes/stub)
endif()

if(LW_PPOCR_BUILD_OPENCV_RUNTIME OR LW_PPOCR_BUILD_DIRECTML_RUNTIME OR
   LW_PPOCR_BUILD_ONNXRUNTIME_RUNTIME OR
   LW_PPOCR_BUILD_OPENVINO_RUNTIME OR LW_PPOCR_BUILD_TENSORRT_RUNTIME)
    if(LW_PPOCR_OPENCV_ROOT)
        find_package(OpenCV CONFIG REQUIRED
            PATHS
                "${LW_PPOCR_OPENCV_ROOT}"
                "${LW_PPOCR_OPENCV_ROOT}/build"
            NO_DEFAULT_PATH
        )
    else()
        find_package(OpenCV CONFIG REQUIRED)
    endif()
    add_subdirectory(src/core/ocr)
endif()

if(LW_PPOCR_BUILD_OPENCV_RUNTIME)
    add_subdirectory(src/runtimes/opencv_dnn)
endif()

if(LW_PPOCR_BUILD_DIRECTML_RUNTIME)
    if(NOT WIN32)
        message(FATAL_ERROR "The DirectML Runtime is available on Windows only")
    endif()
    if(NOT LW_PPOCR_ONNXRUNTIME_DML_ROOT)
        message(FATAL_ERROR "LW_PPOCR_ONNXRUNTIME_DML_ROOT is required")
    endif()
    add_subdirectory(src/runtimes/directml)
endif()

if(LW_PPOCR_BUILD_ONNXRUNTIME_RUNTIME)
    if(NOT LW_PPOCR_ONNXRUNTIME_ROOT)
        message(FATAL_ERROR "LW_PPOCR_ONNXRUNTIME_ROOT is required")
    endif()
    add_subdirectory(src/runtimes/onnxruntime)
endif()

if(LW_PPOCR_BUILD_OPENVINO_RUNTIME)
    if(NOT LW_PPOCR_OPENVINO_ROOT)
        message(FATAL_ERROR "LW_PPOCR_OPENVINO_ROOT is required")
    endif()
    find_package(OpenVINO CONFIG REQUIRED
        PATHS "${LW_PPOCR_OPENVINO_ROOT}/runtime/cmake"
        NO_DEFAULT_PATH
    )
    add_subdirectory(src/runtimes/openvino)
endif()

if(LW_PPOCR_BUILD_TENSORRT_RUNTIME)
    if(NOT WIN32)
        message(FATAL_ERROR "The initial Linux release supports OpenCV DNN only")
    endif()
    if(NOT LW_PPOCR_TENSORRT_ROOT)
        message(FATAL_ERROR "LW_PPOCR_TENSORRT_ROOT is required")
    endif()
    find_package(CUDAToolkit REQUIRED)
    add_subdirectory(src/runtimes/tensorrt)
endif()

if(LW_PPOCR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
