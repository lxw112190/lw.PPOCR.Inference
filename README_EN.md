# lw.PPOCR.Inference

[简体中文](README.md) | [English](README_EN.md)

Author: **天天代码码天天** · QQ: `819069052`

`lw.PPOCR.Inference` is a unified PP-OCR inference project. It exposes one stable C ABI and isolates OpenCV DNN, ONNX Runtime DirectML, portable ONNX Runtime, OpenVINO, and TensorRT as independent Runtime plugins. Windows keeps its four established Runtimes; Linux formally supports OpenCV DNN, ONNX Runtime, and OpenVINO CPU packages.

The current project release is **v1.3.0**. API v1 and its ABI remain frozen. The established Windows deployment baseline remains v1.1.0, while v1.3.0 adds the three formal Linux packages without breaking v1.0.0/v1.1.0 callers. See the [v1.1.0 Windows release notes](docs/releases/v1.1.0.md) and the Linux deployment guides below.

The current Linux release is **v1.3.0**. One GitHub Release provides three independently deployable HTTP service packages—OpenCV DNN, ONNX Runtime, and OpenVINO—and all three have passed CI and Ubuntu 20.04 VM validation. See the [v1.3.0 Release Notes](docs/releases/v1.3.0.md).

| v1.3.0 Linux package | Device | Description | Guide |
|---|---|---|---|
| `linux-x64-opencv` | CPU | Simple general CPU baseline with OpenCV 5.0 | [OpenCV DNN](docs/linux-opencv.md) |
| `linux-arm64-opencv` (v1.4.0-preview.1) | CPU | openEuler 22.03 LTS-SP1 AArch64, generic ARMv8-A | [openEuler ARM64 OpenCV DNN](docs/linux-opencv-openeuler-arm64.md) |
| `linux-x64-onnxruntime-cpu` | CPU / NVIDIA CUDA | Bundles ORT 1.26.0 CPU; accepts a complete matching GPU `.so` set | [ONNX Runtime](docs/linux-onnxruntime.md) |
| `linux-x64-openvino-cpu` | CPU | Bundles OpenVINO 2025.2.0 for Intel/AMD x64 CPUs | [OpenVINO](docs/linux-openvino.md) |

All packages expose the same C ABI and HTTP API, but their private shared libraries must not be mixed. Each Release asset includes a `.tar.gz` archive and matching `.tar.gz.sha256`; transfer and extract the archive on Linux to preserve executable permissions.

Applications only change backend configuration when switching inference frameworks. They do not need to rewrite the OCR workflow. In addition to C#, any language capable of calling a C DLL can integrate with the project, including C, C++, Python, Java, Delphi, Go, and Rust.

## Features

- One API for five inference backends
- A language-neutral C ABI that does not expose STL, `cv::Mat`, C++ classes, exceptions, or compiler-specific types
- Isolated Runtime dependencies that avoid DLL conflicts
- Complete text detection, direction classification, and text recognition pipeline
- Single and batched recognition-only calls for pre-cropped text-line images
- Structured results, JSON results, quadrilateral boxes, confidence scores, and per-stage timings
- BGR, RGB, BGRA, RGBA, and grayscale image inputs
- .NET binding, unified CLI, and WinForms demo
- JSON + Base64 HTTP API, browser test page, Docker Compose, Windows Service mode, and Linux systemd integration
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

Version 0.2.0 added lifecycle, concurrent, and real-model stress coverage for all
four backends. Version 0.3.0 froze the model manifest schema at v1 and added a
golden correctness suite. Version 1.0.0 formally froze API v1 and its ABI. See
[stability testing](docs/stability-testing.md) for stress commands and resource limits.

## Backends

| Backend | Device | Main advantage | Recommended use |
|---|---|---|---|
| OpenCV DNN | CPU | Simple dependencies and broad compatibility | General desktop software and lightweight deployment |
| ONNX Runtime DirectML | CPU / GPU | Uses Windows DirectML across multiple GPU vendors | Intel, AMD, and NVIDIA GPU deployment |
| OpenVINO | CPU | Mature Intel CPU optimization and stable memory policy | CPU inference and concurrent services |
| TensorRT | NVIDIA GPU | High FP16 performance and low latency | Fixed NVIDIA GPU deployments |
| ONNX Runtime (Linux) | CPU / NVIDIA CUDA | One official runtime API for CPU and CUDA, with optional CPU fallback | Linux services and NVIDIA deployment |

The current stable OpenVINO Runtime uses CPU. DirectML can use either CPU or GPU from the demo. TensorRT always requires an NVIDIA GPU.

The Linux OpenCV package bundles minimal OpenCV 5.0 shared libraries. The separate Linux ONNX Runtime package uses the official ONNX Runtime 1.26.0 CPU archive for CI and can be switched to the matching GPU archive by replacing the complete `libonnxruntime*.so*` set. The Linux OpenVINO package pins the official OpenVINO 2025.2.0 Ubuntu 20.04 archive and is released as CPU-only. DirectML remains Windows-only.

## Architecture

```text
Any programming language
    -> lw.PPOCR.dll / liblw.PPOCR.so       Stable C ABI, validation, Runtime loading
        -> lw.PPOCR.Runtime.*.dll / .so    Isolated inference backend
            -> OCR Core             Preprocess, DB postprocess, crop, sort, CTC decode
            -> Inference SDK        OpenCV / ORT / OpenVINO / TensorRT
```

Native dependencies are separated by backend:

```text
runtimes/win-x64/opencv/
runtimes/win-x64/directml/
runtimes/win-x64/openvino/
runtimes/win-x64/tensorrt/
runtimes/linux-x64/opencv/
runtimes/linux-x64/onnxruntime/
runtimes/linux-arm64/opencv/
```

This layout prevents OpenCV, ONNX Runtime, OpenVINO, CUDA, and TensorRT libraries from interfering with each other in the application directory.

## Quick Start

The Windows package provides three applications:

- `lw.PPOCR.Demo.exe`: .NET Framework 4.7.2 WinForms graphical demo
- `lw.PPOCR.Cli.exe`: .NET 8 backend-neutral command-line program
- `lw.PPOCR.HttpService.exe`: native HTTP API and Windows Service host

To use the demo:

1. Select an inference backend.
2. Keep the model manifest at `models\ppocrv6-tiny\model.json` for the bundled model.
3. For DirectML, select **Use GPU** and the GPU ID as needed.
4. Enable **Direction classification CLS** when required.
5. Select **Initialize**, choose an image, and select **Recognize**.
6. View structured or plain-text results and copy the recognized text.
7. Drag a rectangle over a text region in the image. Releasing the mouse skips detection and immediately recognizes the selected crop.

Relative model and Runtime paths in the demo are resolved from the EXE directory, independently of the process working directory. `MainForm` uses the standard WinForms Designer layout and can be edited directly in Visual Studio.

Per-backend split packages preselect their included backend and carry the sample model. Run `run-http-service.cmd` for the local HTTP API and browser test page, or `install-service.cmd` to install an automatically started Windows service with administrator approval. See [HTTP API and Windows Service](docs/http-service.md).

For Linux, run `sudo ./install-deps-ubuntu.sh` followed by `./run-http-service.sh`, then open `http://127.0.0.1:8787`. Use `sudo ./install-systemd.sh` for a system service. WinForms is intentionally Windows-only.

The Linux `.tar.gz` uploaded by CI already contains the detector, recognizer and classifier ONNX models, dictionary, `model.json`, sample image, HTML page, HTTP configuration, systemd scripts, and minimal OpenCV 5.0 shared libraries. No files need to be copied separately from the repository; follow the [Linux quick-start instructions](docs/linux-opencv.md#快速体验) after extraction.

## HTTP Service

The HTTP service accepts images as JSON + Base64. It supports the complete detection, optional direction-classification, and recognition pipeline, as well as recognition-only inference for text regions already cropped by the caller. Its browser test page draws detected regions over the source image and displays per-stage timings.

Default address and endpoints:

```text
Browser test page       GET  http://127.0.0.1:8787/
Health check            GET  /health
Complete OCR            POST /api/ocr
Single/batch recognition POST /api/recognize
```

Complete OCR request:

```json
{"image_base64":"data:image/jpeg;base64,..."}
```

Use `/api/recognize` for one pre-cropped text region:

```json
{"image_base64":"data:image/png;base64,..."}
```

Batch recognition accepts up to 256 images:

```json
{"images_base64":["data:image/png;base64,...","data:image/png;base64,..."]}
```

### API Key

API Key authentication is disabled by default. Requests require no credentials while `api_key` is empty in `http-service.json`:

```json
{"api_key":""}
```

For LAN or service deployments, set it to a sufficiently long random value:

```json
{"api_key":"replace-with-your-random-secret"}
```

Once enabled, both `POST /api/ocr` and `POST /api/recognize` require the same value in the `X-API-Key` request header; otherwise, the service returns HTTP 401. `GET /health` remains unauthenticated for health probes.

```bash
curl -X POST http://127.0.0.1:8787/api/ocr \
  -H "Content-Type: application/json" \
  -H "X-API-Key: replace-with-your-random-secret" \
  --data-binary '{"image_base64":"..."}'
```

The browser page has an API Key field and sends it through the same `X-API-Key` header. It is not placed in the URL or persisted after a page refresh. Startup logs report only whether authentication is enabled and never print the secret value.

The default listener is `127.0.0.1`. For LAN access, change `listen_host` to `0.0.0.0` and also configure an API Key, the operating-system firewall, and trusted-network restrictions. Direct public Internet exposure is not recommended.

On Windows, use `run-http-service.cmd` for foreground execution or run `install-service.cmd` as administrator to install a Windows Service. On Linux, use `./run-http-service.sh` in the foreground or `sudo ./install-systemd.sh` to install under `/opt/lw-ppocr` as a systemd service. See [HTTP API, Windows Service, and Linux systemd](docs/http-service.md) for response fields and detailed deployment instructions.

### Docker Compose

The repository includes a `Dockerfile` and `compose.yaml`. By default, the image
downloads and verifies the formal `v1.3.0 linux-x64-opencv` package. The models,
OpenCV DNN Runtime, HTTP service, and browser test page are included, and OpenCV
is not rebuilt while the image is built:

```bash
cp .env.example .env
# Set LW_PPOCR_API_KEY in .env before LAN or reverse-proxy deployment.
docker compose up -d --build
docker compose logs -f http-service
```

Open `http://127.0.0.1:8787/` after startup. The default container is Linux
x86_64 CPU-only. See [Docker / Docker Compose deployment](docs/docker.md) for
ports, API Key configuration, custom configuration files, health checks, and
security guidance.

### Linux OpenCV DNN HTTP Service Quick Start

The GitHub Actions Artifact ZIP contains the release `.tar.gz` and its `.sha256` file. Copy those two files to Ubuntu 20.04 x86_64 and run:

```bash
sha256sum -c lw.PPOCR.Inference-v1.3.0-linux-x64-opencv.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.3.0-linux-x64-opencv.tar.gz
cd lw.PPOCR.Inference-v1.3.0-linux-x64-opencv
sudo ./install-deps-ubuntu.sh
./verify-linux-package.sh
./run-http-service.sh
```

Open `http://127.0.0.1:8787/`. The verification script checks packaged hashes and ELF dependencies, probes the health endpoint, and runs a real OCR request. Do not run another service on port 8787 during verification.

Before a permanent deployment, edit `http-service.json`. For LAN access, set `listen_host` to `0.0.0.0`, configure a non-empty `api_key`, restrict the firewall to trusted clients, and send the secret through the `X-API-Key` request header. Then install the systemd service:

```bash
sudo ./install-systemd.sh
systemctl status lw-ppocr-http.service
journalctl -u lw-ppocr-http.service -f
```

See the [complete Linux OpenCV DNN deployment guide](docs/linux-opencv.md) for API examples, configuration fields, service management, and troubleshooting.

### Linux ONNX Runtime CPU/CUDA

CI builds this package against the official ONNX Runtime 1.26.0 CPU archive. It is immediately usable on CPU. To enable NVIDIA CUDA, replace the complete `libonnxruntime*.so*` set with files from the matching official 1.26.0 GPU archive, then set `backend_options.device` to `cuda` or `auto`. Never replace only `libonnxruntime.so`, and never mix versions or CUDA 12/CUDA 13 packages.

```json
{
  "backend": "onnxruntime",
  "device_id": 0,
  "backend_options": { "device": "auto" }
}
```

`cpu` forces CPU, `cuda` requires CUDA and fails fast when unavailable, and `auto` attempts CUDA before falling back to CPU. See the [complete Linux ONNX Runtime guide](docs/linux-onnxruntime.md).

### Linux OpenVINO CPU

The CI artifact bundles OpenVINO 2025.2.0 CPU Runtime, oneTBB, the ONNX frontend, models, browser page, and systemd files. It can be verified and started without installing an OpenVINO SDK:

```bash
sha256sum -c lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu.tar.gz
cd lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu
sudo ./install-deps-ubuntu.sh
./verify-linux-package.sh
./run-http-service.sh
```

The release accepts `"backend":"openvino"` and `"backend_options":{"device":"CPU"}` only. See the [complete Linux OpenVINO guide](docs/linux-openvino.md) for API Key, recognition-only, systemd, LAN, and troubleshooting instructions.

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

Backend names are `opencv`, `directml`, `openvino`, `tensorrt`, and `onnxruntime` (alias `ort`).

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

Split packages per backend:

```powershell
.\scripts\package-win-x64.ps1 -Split
```

Each backend package includes a lightweight .NET Framework 4.7.2 WinForms demo, the sample model, the native HTTP service, browser test page, and Windows Service install/uninstall scripts.

Create the official unified ZIP and SHA-256 file:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\package-win-x64.ps1 -Archive
```

Create versioned ZIP and SHA-256 files for the core and all four backend packages:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\package-win-x64.ps1 -Split -Archive
```

Release attachments are written to `dist/releases/v1.1.0/`.

Select specific backends only:

```powershell
.\scripts\package-win-x64.ps1 -Runtime opencv,directml
```

## Code examples

- [C example](examples/c/) — `LoadLibrary` + `GetProcAddress`, zero external dependencies
- [Python example](examples/python/) — `ctypes` wrapper with `with` statement and `run_file()`

## Public C API

The public header is [ppocr_api.h](include/lw/ppocr/ppocr_api.h). The core lifecycle is:

```text
lw_ppocr_create
    -> lw_ppocr_run / lw_ppocr_run_json
        -> lw_ppocr_result_free / lw_ppocr_string_free
    -> lw_ppocr_recognize / lw_ppocr_recognize_batch
        -> lw_ppocr_recognition_result_free
    -> lw_ppocr_destroy
```

`lw_ppocr_recognize_batch` skips detection and preserves input order through
`source_index`. Direction classification remains controlled by `enable_cls`.
These are append-only v1.1.0 additions; the API version remains 1.

Memory returned by the DLL must be released with its corresponding API function. Do not call `delete` or `free` across module boundaries.

## Repository Layout

```text
include/                 Public C API
src/core/                Shared OCR algorithms and image processing
src/loader/              lw.PPOCR.dll and Runtime discovery
src/runtime/             Private Runtime contract
src/runtimes/            Backend implementations
bindings/dotnet/         .NET binding, CLI, and WinForms demo
examples/c/              C language example (LoadLibrary dynamic loading)
examples/python/         Python ctypes binding example
models/                  Model manifests, schema, and experience model
tests/                   ABI, lifecycle, golden, and backend integration tests
docs/                    Architecture, migration, performance, and TensorRT docs
scripts/                 Packaging scripts
.github/workflows/       CI workflows
```

## openEuler ARM64 OpenCV DNN Preview

The `v1.4.0-preview.1` CI target is built for **openEuler 22.03 LTS-SP1 on AArch64/ARM64**. A native GitHub ARM64 runner loads the official openEuler container, compiles OpenCV 5.0, the Runtime, and the HTTP service for the generic `ARMv8-A` baseline, and runs ABI, real OCR, recognition-only, HTTP, ELF architecture, and shared-library checks.

The CI Artifact ZIP contains the release archive and its SHA256 file:

```bash
sha256sum -c lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz
cd lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv
sudo ./install-deps-openeuler.sh
./verify-linux-package.sh
./run-http-service.sh
```

Open `http://127.0.0.1:8787/`. For permanent deployment, edit `http-service.json` and then run `sudo ./install-systemd.sh`. LAN deployment should bind to `0.0.0.0`, configure a non-empty `api_key`, and send the secret through the `X-API-Key` request header. See the [complete openEuler ARM64 guide](docs/linux-opencv-openeuler-arm64.md).

The installed OpenCV tree has a dedicated CI cache and is rebuilt only when the OpenCV version, target userspace, compiler/ARM baseline, or module set changes. The preview does not support ARM32 and intentionally avoids `-march=native`, so the package is not tied to the runner's specific ARM CPU.

The domestic Linux ARM64 validation matrix now covers openEuler 22.03 LTS-SP1, Anolis OS 8.10, and OpenCloudOS 9.4 with vendor-provided ARM64 images and post-package real OCR tests. Kylin V10 and UnionTech OS Server V20 are prepared for official vendor images or labeled self-hosted runners; third-party repackaged images are not treated as release evidence. See [Domestic Linux ARM64 CI and compatibility scope](docs/linux-domestic-arm64-ci.md).

## License

`lw.PPOCR.Inference` is licensed under the [MIT License](LICENSE).

Clipper 6.4.2 under `third_party/clipper` was developed by Angus Johnson and is distributed under the Boost Software License 1.0. Its license is stored at `third_party/clipper/LICENSE.txt` in the source tree and `licenses/clipper-BSL-1.0.txt` in the Windows package. Other third-party components remain subject to their respective licenses and distribution terms.

Windows packages also carry license or third-party notice materials for OpenCV,
ONNX Runtime, DirectML, OpenVINO, TensorRT, CUDA, cpp-httplib, nlohmann/json, and the PP-OCR model artifacts
under `licenses/`. TensorRT and CUDA components are provided by NVIDIA
Corporation; their use and redistribution are subject to the referenced or
included NVIDIA agreements. Use a split package without NVIDIA components if
you do not accept those terms.

## Project Status

The four established Windows Runtimes and all three Linux v1.3.0 packages have passed end-to-end tests with real models. Linux OpenCV DNN, ONNX Runtime, and OpenVINO CPU have passed CI and Ubuntu 20.04 VM validation.

See the [performance report](docs/performance-report.md) for the test hardware, SDK versions, model and image hashes, parameters, and multi-run statistics for all four backends.

The current Linux release is **v1.3.0**, with formal OpenCV DNN, ONNX Runtime CPU/CUDA, and OpenVINO CPU deployment packages. `LW_PPOCR_API_VERSION` remains `1`, and the v1.0.0 ABI freeze remains in force; the Linux Runtimes do not renumber existing backend values or change public structure layouts.
