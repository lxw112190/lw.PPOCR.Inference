# Architecture

## Decisions

1. `lw.PPOCR.dll` is the only native library that language bindings call.
2. Vendor inference libraries are loaded only through isolated Runtime folders.
3. Shared OCR code is a static library linked into each Runtime package.
4. No C++ object crosses a DLL boundary; both public and private plugin contracts
   use versioned C structures and function tables.
5. Configuration belongs to an engine instance. There are no mutable global OCR
   flags.
6. A result must be released before its owning engine is destroyed.
7. Benchmark comparisons run each backend in a separate worker process.

## Components

### Public loader

`lw.PPOCR.dll` validates ABI structures, resolves a Runtime by backend, loads it
with an explicit DLL search directory, and verifies the Runtime API version,
public structure sizes, and ABI fingerprint before forwarding calls. It does not
link against OpenCV, ONNX Runtime, OpenVINO, CUDA, or TensorRT.

### Core static library

The Core owns model-independent OCR behavior: configuration validation,
image conversion, DB postprocessing, crop rotation, box sorting, CTC decoding,
ratio buckets, result construction, diagnostics, and timing.

### Runtime packages

Each Runtime owns all backend state: model sessions, device selection, tensor
buffers, streams, request pools, synchronization, and vendor-specific tuning.
It statically links the Core source but ships in its own folder with only its
required vendor libraries.

The DirectML Runtime creates one ONNX Runtime session per recognition worker,
uses sequential execution within each session, and serializes calls at engine
scope. With `backend_options_json_utf8={"use_gpu":1}`, it appends the DirectML
provider and disables memory-pattern reuse. With `{"use_gpu":0}`, it leaves the
default ONNX Runtime CPU provider active. This preserves the proven DirectML
memory behavior while still allowing independent recognition sessions to
process ratio buckets concurrently. Its packaged `DirectML.dll` must stay beside
the Runtime and ONNX Runtime libraries to make deployment deterministic.

The OpenVINO Runtime is CPU-only in the current stable milestone. Detector and
classifier use low-latency compiled models. Recognition workers share one
compiled model, own separate infer requests and temporary input buffers, and
use a throughput scheduling hint. The Runtime clamps recognition concurrency to
eight and rejects GPU selection at creation instead of exposing the reproduced
OpenCL mapping failure during inference.

The TensorRT Runtime consumes prebuilt engine artifacts. One deserialized
`ICudaEngine` is shared by the recognition workers; every worker owns an
`IExecutionContext`, nonblocking CUDA stream, pinned host buffers, and device
buffers. Buffers grow only when a larger profile shape is observed and are
released with the engine instance. Engine conversion remains an offline
deployment step because TensorRT plans are tied to the target GPU and software
stack.

### Language bindings

Bindings translate native structures into idiomatic language objects and own
marshalling, safe handles, UTF-8 conversion, error translation, and result
release. They do not implement OCR algorithms.

## Runtime layout

```text
runtimes/win-x64/
  opencv/
    lw.PPOCR.Runtime.OpenCVDNN.dll
  directml/
    lw.PPOCR.Runtime.DirectML.dll
    onnxruntime.dll
    onnxruntime_providers_shared.dll
    DirectML.dll
  openvino/
    lw.PPOCR.Runtime.OpenVINO.dll
    openvino.dll
    openvino_onnx_frontend.dll
    openvino_intel_cpu_plugin.dll
    tbb12.dll
    tbbmalloc.dll
  tensorrt/
    lw.PPOCR.Runtime.TensorRT.dll
    nvinfer_10.dll
    nvinfer_plugin_10.dll
    cudart64_12.dll
```

The loader accepts an explicit Runtime library path for tests and advanced
embedding. Normal applications select only a backend and use the standard
Runtime layout.

## Compatibility policy

The product version, public API version, private Runtime API version, and model
manifest schema version are independent. Every extensible structure starts with
`struct_size` and `api_version`. New fields are appended; existing fields are not
reordered or retyped after API v1 is frozen.
