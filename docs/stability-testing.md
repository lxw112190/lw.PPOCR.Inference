# Stability Testing

Version 0.2.0 adds repeatable stability gates for the public ABI and all four
production Runtimes.

## Threading Contract

- Different engine handles are independent and may run concurrently.
- `lw_ppocr_run` and `lw_ppocr_run_json` may be called concurrently on one
  handle. A Runtime may serialize those calls internally.
- Results and strings must be freed with the same handle that created them.
- `lw_ppocr_destroy` is not concurrent. The caller must finish all API calls and
  free every outstanding result or string before destroying an engine.

## ABI Stability Gate

The Stub Runtime test performs:

- 1,000 create/run/destroy lifecycle iterations;
- 10,000 sequential inference calls on one persistent handle;
- 8,000 calls from eight threads sharing one handle;
- eight simultaneous engine instances used by separate threads;
- repeated structured-result and JSON allocation/free operations;
- malformed image validation;
- Windows private-memory and process-handle growth checks.

Run it with:

```powershell
ctest --test-dir build\ninja-x64 -C Release `
  -R lw.ppocr.abi.stability --output-on-failure
```

## Production Runtime Gate

Each OpenCV DNN, DirectML, OpenVINO, and TensorRT integration test initializes a
real model, warms the Runtime, validates OCR output, runs 100 sequential calls,
then runs 40 calls from four threads sharing the engine. The test verifies a
stable region count and checks process resource growth after warmup.

```powershell
ctest --test-dir build\ninja-x64 -C Release `
  -L stability --output-on-failure
```

The integration executable accepts these environment variables for local soak
tests:

```powershell
$env:LW_PPOCR_STRESS_ITERATIONS = "1000"
$env:LW_PPOCR_STRESS_THREADS = "8"
$env:LW_PPOCR_STRESS_RUNS_PER_THREAD = "100"
ctest --test-dir build\ninja-x64 -C Release `
  -L integration --output-on-failure
```

The checked-in defaults allow up to 256 MiB of private-memory growth and 32
additional process handles after model warmup. GPU SDKs may retain lazy caches,
so this is a regression guard rather than a claim that every allocator returns
memory to Windows immediately.
