add_executable(lw_ppocr_abi_smoke_test
    abi_smoke_test.cpp
)
add_executable(lw_ppocr_abi_stability_test
    abi_stability_test.cpp
)
find_package(Threads REQUIRED)
target_include_directories(lw_ppocr_abi_smoke_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(lw_ppocr_abi_stability_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(lw_ppocr_abi_smoke_test PRIVATE lw_ppocr_loader)
target_link_libraries(lw_ppocr_abi_stability_test
    PRIVATE lw_ppocr_loader Threads::Threads
)

if(WIN32)
    target_link_libraries(lw_ppocr_abi_stability_test PRIVATE Psapi)
endif()

if(MSVC)
    target_compile_options(lw_ppocr_abi_smoke_test PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(lw_ppocr_abi_stability_test PRIVATE /W4 /permissive- /utf-8)
endif()

if(TARGET lw_ppocr_runtime_stub)
    add_test(
        NAME lw.ppocr.abi.smoke
        COMMAND lw_ppocr_abi_smoke_test $<TARGET_FILE:lw_ppocr_runtime_stub>
    )
    add_test(
        NAME lw.ppocr.abi.stability
        COMMAND lw_ppocr_abi_stability_test $<TARGET_FILE:lw_ppocr_runtime_stub>
    )
    if(TARGET lw_ppocr_runtime_stub_forward)
        add_test(
            NAME lw.ppocr.abi.forward_compatibility
            COMMAND lw_ppocr_abi_smoke_test
                $<TARGET_FILE:lw_ppocr_runtime_stub_forward>
        )
        set_tests_properties(lw.ppocr.abi.forward_compatibility PROPERTIES
            LABELS "abi"
        )
    endif()
    set_tests_properties(lw.ppocr.abi.stability PROPERTIES LABELS "stability")
endif()

if(TARGET lw_ppocr_runtime_opencv_dnn OR
   TARGET lw_ppocr_runtime_directml OR
   TARGET lw_ppocr_runtime_onnxruntime OR
   TARGET lw_ppocr_runtime_openvino OR
   TARGET lw_ppocr_runtime_tensorrt)
    add_executable(lw_ppocr_opencv_integration_test
        opencv_integration_test.cpp
    )
    target_include_directories(lw_ppocr_opencv_integration_test
        PRIVATE ${CMAKE_SOURCE_DIR}/include ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(lw_ppocr_opencv_integration_test
        PRIVATE lw_ppocr_loader ${OpenCV_LIBS} Threads::Threads
    )
    if(WIN32)
        target_link_libraries(lw_ppocr_opencv_integration_test PRIVATE Psapi)
    endif()

    if(MSVC)
        target_compile_options(lw_ppocr_opencv_integration_test
            PRIVATE /W4 /permissive- /utf-8 /wd4996
        )
    endif()

    if(TARGET lw_ppocr_runtime_directml AND
       LW_PPOCR_TEST_DET_MODEL AND LW_PPOCR_TEST_REC_MODEL AND
       LW_PPOCR_TEST_DICTIONARY AND LW_PPOCR_TEST_IMAGE)
        add_test(
            NAME lw.ppocr.directml.integration
            COMMAND lw_ppocr_opencv_integration_test
                $<TARGET_FILE:lw_ppocr_runtime_directml>
                "${LW_PPOCR_TEST_DET_MODEL}"
                "${LW_PPOCR_TEST_REC_MODEL}"
                "${LW_PPOCR_TEST_DICTIONARY}"
                "${LW_PPOCR_TEST_IMAGE}"
                directml
                "${LW_PPOCR_TEST_DIRECTML_DEVICE_ID}"
                "${LW_PPOCR_TEST_CLS_MODEL}"
        )
        set_tests_properties(lw.ppocr.directml.integration PROPERTIES
            ENVIRONMENT "LW_PPOCR_STRESS_ITERATIONS=100;LW_PPOCR_STRESS_THREADS=4;LW_PPOCR_STRESS_RUNS_PER_THREAD=10"
            LABELS "integration;stability"
            TIMEOUT 300
        )
    endif()

    if(TARGET lw_ppocr_runtime_onnxruntime AND
       LW_PPOCR_TEST_DET_MODEL AND LW_PPOCR_TEST_REC_MODEL AND
       LW_PPOCR_TEST_DICTIONARY AND LW_PPOCR_TEST_IMAGE)
        add_test(
            NAME lw.ppocr.onnxruntime.integration
            COMMAND lw_ppocr_opencv_integration_test
                $<TARGET_FILE:lw_ppocr_runtime_onnxruntime>
                "${LW_PPOCR_TEST_DET_MODEL}"
                "${LW_PPOCR_TEST_REC_MODEL}"
                "${LW_PPOCR_TEST_DICTIONARY}"
                "${LW_PPOCR_TEST_IMAGE}"
                onnxruntime
                cpu
                "${LW_PPOCR_TEST_CLS_MODEL}"
        )
        set_tests_properties(lw.ppocr.onnxruntime.integration PROPERTIES
            ENVIRONMENT "LW_PPOCR_STRESS_ITERATIONS=100;LW_PPOCR_STRESS_THREADS=4;LW_PPOCR_STRESS_RUNS_PER_THREAD=10"
            LABELS "integration;stability"
            TIMEOUT 300
        )
        if(WIN32 AND TARGET opencv_world)
            set_property(TEST lw.ppocr.onnxruntime.integration APPEND PROPERTY
                ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:opencv_world>"
            )
        endif()
    endif()

    if(TARGET lw_ppocr_runtime_openvino AND
       LW_PPOCR_TEST_DET_MODEL AND LW_PPOCR_TEST_REC_MODEL AND
       LW_PPOCR_TEST_DICTIONARY AND LW_PPOCR_TEST_IMAGE)
        add_test(
            NAME lw.ppocr.openvino.integration
            COMMAND lw_ppocr_opencv_integration_test
                $<TARGET_FILE:lw_ppocr_runtime_openvino>
                "${LW_PPOCR_TEST_DET_MODEL}"
                "${LW_PPOCR_TEST_REC_MODEL}"
                "${LW_PPOCR_TEST_DICTIONARY}"
                "${LW_PPOCR_TEST_IMAGE}"
                openvino
                "${LW_PPOCR_TEST_OPENVINO_DEVICE}"
                "${LW_PPOCR_TEST_CLS_MODEL}"
        )
        set_tests_properties(lw.ppocr.openvino.integration PROPERTIES
            ENVIRONMENT "LW_PPOCR_STRESS_ITERATIONS=100;LW_PPOCR_STRESS_THREADS=4;LW_PPOCR_STRESS_RUNS_PER_THREAD=10"
            LABELS "integration;stability"
            TIMEOUT 300
        )
    endif()

    if(TARGET lw_ppocr_runtime_tensorrt AND
       LW_PPOCR_TEST_TENSORRT_DET_ENGINE AND
       LW_PPOCR_TEST_TENSORRT_REC_ENGINE AND
       LW_PPOCR_TEST_TENSORRT_DICTIONARY AND
       LW_PPOCR_TEST_TENSORRT_IMAGE)
        add_test(
            NAME lw.ppocr.tensorrt.integration
            COMMAND lw_ppocr_opencv_integration_test
                $<TARGET_FILE:lw_ppocr_runtime_tensorrt>
                "${LW_PPOCR_TEST_TENSORRT_DET_ENGINE}"
                "${LW_PPOCR_TEST_TENSORRT_REC_ENGINE}"
                "${LW_PPOCR_TEST_TENSORRT_DICTIONARY}"
                "${LW_PPOCR_TEST_TENSORRT_IMAGE}"
                tensorrt
                "${LW_PPOCR_TEST_TENSORRT_DEVICE_ID}"
                "${LW_PPOCR_TEST_TENSORRT_CLS_ENGINE}"
        )
        set_tests_properties(lw.ppocr.tensorrt.integration PROPERTIES
            ENVIRONMENT "LW_PPOCR_STRESS_ITERATIONS=100;LW_PPOCR_STRESS_THREADS=4;LW_PPOCR_STRESS_RUNS_PER_THREAD=10"
            LABELS "integration;stability"
            TIMEOUT 300
        )
    endif()

    if(TARGET lw_ppocr_runtime_opencv_dnn AND
       LW_PPOCR_TEST_DET_MODEL AND LW_PPOCR_TEST_REC_MODEL AND
       LW_PPOCR_TEST_DICTIONARY AND LW_PPOCR_TEST_IMAGE)
        add_test(
            NAME lw.ppocr.opencv.integration
            COMMAND lw_ppocr_opencv_integration_test
                $<TARGET_FILE:lw_ppocr_runtime_opencv_dnn>
                "${LW_PPOCR_TEST_DET_MODEL}"
                "${LW_PPOCR_TEST_REC_MODEL}"
                "${LW_PPOCR_TEST_DICTIONARY}"
                "${LW_PPOCR_TEST_IMAGE}"
        )
        set_tests_properties(lw.ppocr.opencv.integration PROPERTIES
            ENVIRONMENT "LW_PPOCR_STRESS_ITERATIONS=100;LW_PPOCR_STRESS_THREADS=4;LW_PPOCR_STRESS_RUNS_PER_THREAD=10"
            LABELS "integration;stability"
            TIMEOUT 300
        )
    endif()

    if(TARGET lw_ppocr_runtime_opencv_dnn AND
       LW_PPOCR_TEST_DET_MODEL AND LW_PPOCR_TEST_REC_MODEL AND
       LW_PPOCR_TEST_DICTIONARY AND LW_PPOCR_TEST_IMAGE)
        add_executable(lw_ppocr_golden_test
            golden_test.cpp
        )
        target_include_directories(lw_ppocr_golden_test
            PRIVATE ${CMAKE_SOURCE_DIR}/include ${OpenCV_INCLUDE_DIRS}
        )
        target_link_libraries(lw_ppocr_golden_test
            PRIVATE lw_ppocr_loader ${OpenCV_LIBS}
        )
        if(MSVC)
            target_compile_options(lw_ppocr_golden_test
                PRIVATE /W4 /permissive- /utf-8
            )
        endif()

        set(LW_PPOCR_GOLDEN_FILE
            "${CMAKE_SOURCE_DIR}/tests/golden/sample.jpg.golden.json"
        )
        set(LW_PPOCR_TEST_CLS_ARG
            "${LW_PPOCR_TEST_CLS_MODEL}"
        )
        if(NOT LW_PPOCR_TEST_CLS_MODEL)
            set(LW_PPOCR_TEST_CLS_ARG "none")
        endif()

        add_test(
            NAME lw.ppocr.golden
            COMMAND lw_ppocr_golden_test
                $<TARGET_FILE:lw_ppocr_runtime_opencv_dnn>
                "${LW_PPOCR_TEST_DET_MODEL}"
                "${LW_PPOCR_TEST_REC_MODEL}"
                "${LW_PPOCR_TEST_DICTIONARY}"
                "${LW_PPOCR_TEST_IMAGE}"
                "${LW_PPOCR_TEST_CLS_ARG}"
                "${LW_PPOCR_GOLDEN_FILE}"
        )
        set_tests_properties(lw.ppocr.golden PROPERTIES
            LABELS "golden"
            TIMEOUT 120
        )
    endif()
endif()
