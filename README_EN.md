# lw.PPOCR.Inference

[简体中文](README.md) | [English](README_EN.md)

`lw.PPOCR.Inference` is a unified PP-OCR inference project for Windows. It exposes one stable C ABI and isolates OpenCV DNN, ONNX Runtime DirectML, OpenVINO, and TensorRT as independent Runtime plugins.

Applications only change backend configuration when switching inference frameworks. They do not need to rewrite the OCR workflow. In addition to C#, any language capable of calling a C DLL can integrate with the project, including C, C++, Python, Java, Delphi, Go, and Rust.

## Features

- One API for four inference backends
- A language-neutral C ABI that does not expose STL, `cv::Mat`, C++ classes, exceptions, or compiler-specific types
- Isolated Runtime dependencies that avoid DLL conflicts
- Complete text detection, direction classification, and text recognition pipeline
- Structured results, JSON results, quadrilateral boxes, confidence scores, and per-stage timings
- BGR, RGB, BGRA, RGBA, and grayscale image inputs
- .NET binding, unified CLI, and WinForms demo
- Model packages described by `model.json`, without hard-coded artifact names
- Instance-owned configuration, workers, and memory with explicit initialization and destruction

## Thread Safety and Stability

- Different OCR engine instances are independent and may run concurrently.
- One instance accepts concurrent `Run` calls; a Runtime may serialize inference
  internally.
- Structured results and JSON strings must be freed by the same instance that
  created them.
- `Destroy` must not overlap inference. Finish all calls and free all results
  before destroying the engine.

Version 0.2.0 adds lifecycle, 10,000-call sequential, eight-thread shared-engine,
multiple-instance, and real-model stress coverage for all four backends. See
[stability testing](docs/stability-testing.md) for commands and resource limits.

## Backends

| Backend | Device | Main advantage | Recommended use |
|---|---|---|---|
| OpenCV DNN | CPU | Simple dependencies and broad compatibility | General desktop software and lightweight deployment |
| ONNX Runtime DirectML | CPU / GPU | Uses Windows DirectML across multiple GPU vendors | Intel, AMD, and NVIDIA GPU deployment |
| OpenVINO | CPU | Mature Intel CPU optimization and stable memory policy | CPU inference and concurrent services |
| TensorRT | NVIDIA GPU | High FP16 performance and low latency | Fixed NVIDIA GPU deployments |

The current stable OpenVINO Runtime uses CPU. DirectML can use either CPU or GPU from the demo. TensorRT always requires an NVIDIA GPU.

## Architecture

```text
Any programming language
    -> lw.PPOCR.dll                 Stable C ABI, validation, Runtime loading
        -> lw.PPOCR.Runtime.*.dll   Isolated inference backend
            -> OCR Core             Preprocess, DB postprocess, crop, sort, CTC decode
            -> Inference SDK        OpenCV / ORT / OpenVINO / TensorRT
```

Native dependencies are separated by backend:

```text
runtimes/win-x64/opencv/
runtimes/win-x64/directml/
runtimes/win-x64/openvino/
runtimes/win-x64/tensorrt/
```

This layout prevents OpenCV, ONNX Runtime, OpenVINO, CUDA, and TensorRT libraries from interfering with each other in the application directory.

## Quick Start

The Windows package provides two applications:

- `lw.PPOCR.Demo.exe`: WinForms graphical demo
- `lw.PPOCR.Cli.exe`: backend-neutral command-line program

To use the demo:

1. Select an inference backend.
2. Keep the model manifest at `models\ppocrv6-tiny\model.json` for the bundled model.
3. For DirectML, select **Use GPU** and the GPU ID as needed.
4. Enable **Direction classification CLS** when required.
5. Select **Initialize**, choose an image, and select **Recognize**.
6. View structured or plain-text results and copy the recognized text.

Relative model and Runtime paths in the demo are resolved from the EXE directory, independently of the process working directory. `MainForm` uses the standard WinForms Designer layout and can be edited directly in Visual Studio.

## CLI Examples

```powershell
# OpenCV DNN on CPU
.\lw.PPOCR.Cli.exe opencv models\ppocrv6-tiny\model.json test.jpg

# OpenVINO on CPU
.\lw.PPOCR.Cli.exe openvino models\ppocrv6-tiny\model.json test.jpg

# DirectML on GPU 1 with direction classification
.\lw.PPOCR.Cli.exe directml models\ppocrv6-tiny\model.json test.jpg runtimes\win-x64 1 true

# TensorRT on GPU 0 with direction classification
.\lw.PPOCR.Cli.exe tensorrt models\ppocrv6-tiny\model.json test.jpg runtimes\win-x64 0 true
```

Argument order:

```text
backend  model.json  image  [runtime-root]  [GPU ID]  [CLS]  [warmup]  [runs]
```

Backend names are `opencv`, `directml`, `openvino`, and `tensorrt`.

The optional warmup and run counts enable repeatable benchmarking. The CLI reuses one engine and reports the mean, median, P95, minimum, and maximum latency. Without these arguments, it performs one recognition run as before.

## Model Manifest

`model.json` describes the package name, dictionary, detector, classifier, recognizer, and backend-specific artifacts.

```json
{
  "schema_version": 1,
  "name": "PP-OCRv6 tiny Chinese",
  "dictionary": {
    "path": "ppocr_keys.txt"
  },
  "stages": {
    "detector": {
      "artifacts": {
        "onnx": { "path": "det.onnx", "format": "onnx" },
        "tensorrt": { "path": "det.fp16.engine", "format": "tensorrt-engine" }
      }
    },
    "classifier": {
      "artifacts": {
        "onnx": { "path": "cls.onnx", "format": "onnx" },
        "tensorrt": { "path": "cls.fp16.engine", "format": "tensorrt-engine" }
      }
    },
    "recognizer": {
      "artifacts": {
        "onnx": { "path": "rec.onnx", "format": "onnx" },
        "tensorrt": { "path": "rec.fp16.engine", "format": "tensorrt-engine" }
      }
    }
  }
}
```

Manifest-relative paths are resolved from the directory containing `model.json`. Portable backends select ONNX artifacts, while TensorRT selects prebuilt engine artifacts.

See the complete [model manifest schema](models/model-manifest.schema.json).

## TensorRT FP16 Conversion

The TensorRT Runtime loads serialized engines directly. It does not convert ONNX during application initialization. This reduces startup work and exposes conversion failures before deployment.

### Requirements

1. Install an NVIDIA driver and CUDA version compatible with TensorRT.
2. Extract the TensorRT Windows package.
3. Locate `trtexec.exe` under the TensorRT `bin` directory.
4. Prepare `det.onnx`, `rec.onnx`, `cls.onnx`, and the OCR dictionary.

Update these paths for the target computer:

```powershell
$trtexec = "C:\TensorRT-10.16.1.11\bin\trtexec.exe"
$modelDir = "C:\models\ppocrv6-tiny"
```

The bundled ONNX models use `x` as their input tensor name. Replace `x:` in the shape arguments if another model uses a different input name.

### Detector

```powershell
& $trtexec `
  --onnx="$modelDir\det.onnx" `
  --saveEngine="$modelDir\det.fp16.engine" `
  --fp16 `
  --minShapes=x:1x3x32x32 `
  --optShapes=x:1x3x960x960 `
  --maxShapes=x:1x3x960x960 `
  --builderOptimizationLevel=5 `
  --skipInference
```

The detector maximum shape must cover `limit_side_len`. Increase `maxShapes` when using a detection side length greater than 960. A larger profile can increase engine build time and GPU memory usage.

### Recognizer

```powershell
& $trtexec `
  --onnx="$modelDir\rec.onnx" `
  --saveEngine="$modelDir\rec.fp16.engine" `
  --fp16 `
  --minShapes=x:1x3x48x32 `
  --optShapes=x:4x3x48x320 `
  --maxShapes=x:16x3x48x1280 `
  --builderOptimizationLevel=5 `
  --skipInference
```

This profile supports batches up to 16 and text widths up to 1280. The Runtime reads the engine profile at startup and resizes over-wide crops to the profile maximum instead of passing an invalid shape to TensorRT.

### Direction Classifier

```powershell
& $trtexec `
  --onnx="$modelDir\cls.onnx" `
  --saveEngine="$modelDir\cls.fp16.engine" `
  --fp16 `
  --minShapes=x:1x3x80x160 `
  --optShapes=x:8x3x80x160 `
  --maxShapes=x:16x3x80x160 `
  --builderOptimizationLevel=5 `
  --skipInference
```

The classifier engine is optional when direction classification is disabled.

### Test the Engines

Place the generated files beside `model.json`:

```text
models/ppocrv6-tiny/
  model.json
  ppocr_keys.txt
  det.onnx
  rec.onnx
  cls.onnx
  det.fp16.engine
  rec.fp16.engine
  cls.fp16.engine
```

Run a TensorRT test:

```powershell
.\lw.PPOCR.Cli.exe tensorrt `
  models\ppocrv6-tiny\model.json `
  test.jpg `
  runtimes\win-x64 `
  0 `
  true
```

TensorRT engines are generally tied to the GPU architecture, TensorRT, CUDA, and driver stack used to build them. Production engines should be regenerated on the deployment computer or an identical environment.

See [TensorRT engine preparation](docs/tensorrt-engines.md) for additional profile details.

## Build Requirements

- Windows 10 or Windows 11 x64
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.24 or newer
- .NET 8 SDK
- OpenCV 5.0
- ONNX Runtime DirectML and `DirectML.dll` for the DirectML Runtime
- OpenVINO Toolkit for the OpenVINO Runtime
- CUDA Toolkit and TensorRT for the TensorRT Runtime

Build the Loader, Stub Runtime, and base tests:

```powershell
cmake --preset vs2022-x64
cmake --build --preset vs2022-x64-release
ctest --preset vs2022-x64-release
```

## Build All Runtimes

The following Ninja example enables all production backends. Replace every SDK path with its local location:

```powershell
cmake -S . -B build\ninja-x64 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DLW_PPOCR_BUILD_OPENCV_RUNTIME=ON `
  -DLW_PPOCR_BUILD_DIRECTML_RUNTIME=ON `
  -DLW_PPOCR_BUILD_OPENVINO_RUNTIME=ON `
  -DLW_PPOCR_BUILD_TENSORRT_RUNTIME=ON `
  -DLW_PPOCR_OPENCV_ROOT=C:\SDK\opencv `
  -DLW_PPOCR_ONNXRUNTIME_DML_ROOT=C:\SDK\onnxruntime-directml `
  -DLW_PPOCR_DIRECTML_DLL=C:\SDK\DirectML.dll `
  -DLW_PPOCR_OPENVINO_ROOT=C:\SDK\openvino `
  -DLW_PPOCR_TENSORRT_ROOT=C:\SDK\TensorRT-10.16.1.11 `
  "-DCUDAToolkit_ROOT=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9"

cmake --build build\ninja-x64
ctest --test-dir build\ninja-x64 -C Release --output-on-failure
```

Only required Runtimes need to be enabled. The public application API does not change when a backend is omitted from a build.

## Windows Package

After building the native Runtimes, create the redistributable package:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\package-win-x64.ps1
```

The default output directory is:

```text
dist/lw.PPOCR.Inference-win-x64/
```

The script publishes the Loader, Runtime plugins, .NET CLI, WinForms demo, sample model, licenses, and documentation. It also generates `package-files.sha256`. Repackaging preserves the `test` directory so local test images are not deleted.

## Public C API

The public header is [ppocr_api.h](include/lw/ppocr/ppocr_api.h). The core lifecycle is:

```text
lw_ppocr_create
    -> lw_ppocr_run / lw_ppocr_run_json
        -> lw_ppocr_result_free / lw_ppocr_string_free
    -> lw_ppocr_destroy
```

Memory returned by the DLL must be released with its corresponding API function. Do not call `delete` or `free` across module boundaries.

## Repository Layout

```text
include/                 Public C API
src/core/                Shared OCR algorithms and image processing
src/loader/              lw.PPOCR.dll and Runtime discovery
src/runtime/             Private Runtime contract
src/runtimes/            Backend implementations
bindings/dotnet/         .NET binding, CLI, and WinForms demo
models/                  Model manifests, schema, and experience model
tests/                   ABI, lifecycle, and backend integration tests
docs/                    Architecture, migration, performance, and TensorRT docs
scripts/                 Packaging scripts
```

## License

`lw.PPOCR.Inference` is licensed under the [MIT License](LICENSE).

Clipper 6.4.2 under `third_party/clipper` was developed by Angus Johnson and is distributed under the Boost Software License 1.0. Its license is stored at `third_party/clipper/LICENSE.txt` in the source tree and `licenses/clipper-BSL-1.0.txt` in the Windows package. Other third-party components remain subject to their respective licenses and distribution terms.

## Project Status

All four production Runtimes have passed end-to-end tests with real models. The unified Loader, C ABI, .NET binding, CLI, WinForms demo, benchmark mode, and Windows packaging workflow are operational.

See the [performance report](docs/performance-report.md) for the test hardware, SDK versions, model and image hashes, parameters, and multi-run statistics for all four backends.

The project is currently pre-1.0. Public interfaces may still change before API v1 is frozen. After that point, changes will favor additive fields and structure-size versioning to preserve backward compatibility.
