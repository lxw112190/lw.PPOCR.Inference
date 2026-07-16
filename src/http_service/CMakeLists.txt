add_executable(lw_ppocr_http_service
    http_service.cpp
    base64.cpp
    image_decoder.cpp
    version.rc
)

target_include_directories(lw_ppocr_http_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(lw_ppocr_http_service SYSTEM PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/cpp-httplib
    ${PROJECT_SOURCE_DIR}/third_party/nlohmann-json
)
target_link_libraries(lw_ppocr_http_service PRIVATE
    lw_ppocr_loader
    advapi32
    ole32
    windowscodecs
    ws2_32
)
target_compile_definitions(lw_ppocr_http_service PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)
set_target_properties(lw_ppocr_http_service PROPERTIES
    OUTPUT_NAME "lw.PPOCR.HttpService"
)

if(MSVC)
    target_compile_options(lw_ppocr_http_service PRIVATE
        /W4 /permissive- /utf-8 /bigobj
    )
endif()

install(TARGETS lw_ppocr_http_service RUNTIME DESTINATION .)
install(FILES
    ${PROJECT_SOURCE_DIR}/examples/http/http-service.json
    ${PROJECT_SOURCE_DIR}/examples/http/run-http-service.cmd
    ${PROJECT_SOURCE_DIR}/examples/http/install-service.cmd
    ${PROJECT_SOURCE_DIR}/examples/http/uninstall-service.cmd
    DESTINATION .
)
install(FILES ${PROJECT_SOURCE_DIR}/examples/http/www/index.html
    DESTINATION www
)
