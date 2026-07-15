# Migration Roadmap

## Phase 1: Contract

- Freeze API v1 after OpenCV DNN stability and binding tests are complete.
- Validate UTF-8, image stride, allocation ownership, error propagation, and
  repeated create/run/destroy behavior.
- Establish the model manifest schema and dependency layout.

## Phase 2: OpenCV DNN

- [x] Migrate shared preprocessing and postprocessing into Core.
- [x] Implement the first production Runtime.
- [x] Run a real PP-OCRv5 model through the public Loader ABI.
- [x] Add repeated-run memory and result-count baselines.
- [ ] Compare warm timing against the existing OpenCV DNN demo.

## Phase 3: GPU runtimes

- [x] Migrate DirectML with isolated ONNX Runtime and DirectML dependencies.
- [x] Preserve independent recognition sessions and ratio-bucket concurrency.
- [x] Add DirectML device selection, capabilities, optional classifier, and a
  real-model integration test.
- [x] Migrate OpenVINO as an isolated CPU Runtime with shared compiled models.
- [x] Reproduce and guard the unstable OpenVINO GPU/OpenCL path at creation.
- [x] Migrate TensorRT with shared engines and independent CUDA contexts.
- [x] Add offline engine conversion profiles and a V6 FP16 integration test.
- [ ] Expand backend-specific diagnostics and adapter enumeration.

## Phase 4: Bindings and distribution

- [x] Add the first safe-handle .NET binding and Runtime capability query.
- [x] Add a backend-neutral .NET CLI exercising the public Loader API.
- [x] Add a unified WinForms experience program for all four backends.
- [x] Produce a clean Windows x64 layout with isolated Runtime dependencies and
  a SHA-256 file manifest.
- Complete the .NET SDK and unified WinForms application.
- Add Python and Java bindings without changing the native ABI.
- Publish backend-specific ZIP and package-manager artifacts.

## Phase 5: Benchmark and stability

- Run each backend in an isolated worker process.
- Record cold start, warm P50/P90/P99 latency, throughput, memory, and VRAM.
- [x] Add long-running lifecycle, concurrency, malformed-input, and resource
  growth tests.
- Add golden text and box-coordinate result baselines.
