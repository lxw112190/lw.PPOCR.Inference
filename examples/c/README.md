# C example

Minimal C program demonstrating the `lw.PPOCR.Inference` public C ABI.

## What it shows

- `LoadLibrary` / `GetProcAddress` — no import library needed
- `lw_ppocr_create` → `lw_ppocr_run` → `lw_ppocr_result_free` → `lw_ppocr_destroy`
- Error retrieval via `lw_ppocr_get_last_error`
- Memory ownership rules (free results before destroying engine)

## Build

### MSVC (x64 Native Tools Command Prompt)

```cmd
cl /utf-8 /Fe:ppocr_example.exe main.c
```

No external libraries required — only `kernel32.lib` (always linked) and `lw.PPOCR.dll` at runtime.

### CMake (optional)

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run

Copy `ppocr_example.exe` into the same directory as `lw.PPOCR.dll` and the
`models/`, `runtimes/` folders, then:

```cmd
ppocr_example.exe
```

If the test image (2×2 black pixels) produces no text regions, the program
still succeeds — the goal is to demonstrate the API lifecycle. Replace the
image buffer with a real decoded JPEG/PNG to test with bundled models.

## Adapting for real images

To process an actual image file, replace the inline pixel buffer with decoded
data.  For example, with OpenCV:

```c
/* add #include <opencv2/imgcodecs.hpp> and compile as C++ */
cv::Mat mat = cv::imread("photo.jpg", cv::IMREAD_COLOR);
image.data      = mat.data;
image.data_size = (uint64_t)(mat.step) * mat.rows;
image.width     = mat.cols;
image.height    = mat.rows;
image.stride    = (int32_t)mat.step;
```

Or use any image library that can produce a BGR 8-bit buffer.
