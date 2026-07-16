cmake_minimum_required(VERSION 3.24)
project(ppocr_c_example LANGUAGES C)

add_executable(ppocr_example main.c)

if(MSVC)
    target_compile_options(ppocr_example PRIVATE /W4 /utf-8)
endif()

# If lw.PPOCR.dll is available, add a test that runs the example.
# The test expects the DLL and models to be discoverable at runtime.
if(DEFINED LW_PPOCR_EXAMPLE_RUNTIME_DIR)
    add_test(
        NAME example.c.smoke
        COMMAND ppocr_example
        WORKING_DIRECTORY "${LW_PPOCR_EXAMPLE_RUNTIME_DIR}"
    )
endif()
