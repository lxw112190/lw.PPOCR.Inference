set(LW_PPOCR_HTTP_SOURCES
    http_service.cpp
    base64.cpp
    image_decoder.cpp
)
if(WIN32)
    list(APPEND LW_PPOCR_HTTP_SOURCES version.rc)
endif()
add_executable(lw_ppocr_http_service ${LW_PPOCR_HTTP_SOURCES})

target_include_directories(lw_ppocr_http_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(lw_ppocr_http_service SYSTEM PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/cpp-httplib
    ${PROJECT_SOURCE_DIR}/third_party/nlohmann-json
)
target_link_libraries(lw_ppocr_http_service PRIVATE lw_ppocr_loader)
if(WIN32)
    target_link_libraries(lw_ppocr_http_service PRIVATE
        advapi32 ole32 windowscodecs ws2_32
    )
    target_compile_definitions(lw_ppocr_http_service PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
    )
    set_target_properties(lw_ppocr_http_service PROPERTIES
        OUTPUT_NAME "lw.PPOCR.HttpService"
    )
else()
    find_package(Threads REQUIRED)
    if(NOT OpenCV_FOUND)
        find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc dnn)
    endif()
    target_include_directories(lw_ppocr_http_service SYSTEM PRIVATE
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(lw_ppocr_http_service PRIVATE
        ${OpenCV_LIBS} Threads::Threads
    )
    set_target_properties(lw_ppocr_http_service PROPERTIES
        OUTPUT_NAME "lw-ppocr-http-service"
        BUILD_RPATH "$ORIGIN;$ORIGIN/runtimes/linux-x64/opencv;$ORIGIN/runtimes/linux-x64/onnxruntime"
        INSTALL_RPATH "$ORIGIN;$ORIGIN/runtimes/linux-x64/opencv;$ORIGIN/runtimes/linux-x64/onnxruntime"
    )
endif()

if(MSVC)
    target_compile_options(lw_ppocr_http_service PRIVATE
        /W4 /permissive- /utf-8 /bigobj
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lw_ppocr_http_service PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

install(TARGETS lw_ppocr_http_service RUNTIME DESTINATION .)
if(WIN32)
    install(FILES
        ${PROJECT_SOURCE_DIR}/examples/http/http-service.json
        ${PROJECT_SOURCE_DIR}/examples/http/run-http-service.cmd
        ${PROJECT_SOURCE_DIR}/examples/http/install-service.cmd
        ${PROJECT_SOURCE_DIR}/examples/http/uninstall-service.cmd
        DESTINATION .
    )
else()
    install(FILES ${PROJECT_SOURCE_DIR}/examples/http/http-service-linux.json
        DESTINATION . RENAME http-service.json
    )
    install(PROGRAMS
        ${PROJECT_SOURCE_DIR}/examples/http/run-http-service.sh
        ${PROJECT_SOURCE_DIR}/examples/http/install-systemd.sh
        ${PROJECT_SOURCE_DIR}/examples/http/uninstall-systemd.sh
        ${PROJECT_SOURCE_DIR}/examples/http/install-deps-ubuntu.sh
        ${PROJECT_SOURCE_DIR}/scripts/verify-linux-package.sh
        DESTINATION .
    )
    install(FILES ${PROJECT_SOURCE_DIR}/examples/http/lw-ppocr-http.service
        DESTINATION .
    )
endif()
install(FILES ${PROJECT_SOURCE_DIR}/examples/http/www/index.html
    DESTINATION www
)
