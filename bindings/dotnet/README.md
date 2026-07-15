# .NET binding

`lw.PPOCRSharp` is the first ergonomic language binding for
`lw.PPOCR.Inference`. It calls only `lw.PPOCR.dll`; it does not link directly to
OpenCV, ONNX Runtime, OpenVINO, CUDA, or TensorRT.

The initial `net8.0` API accepts a managed byte array with explicit dimensions,
stride, and pixel format. Additional target frameworks will be added after the
native API v1 is frozen. UI-specific conversions such as `Bitmap`, WPF, or
OpenCvSharp belong in optional adapter packages rather than the core binding.

`OcrEngine` owns the native engine through a safe handle and releases every
native result after converting it to managed records. Its `Capabilities`
property reports the selected Runtime name and CPU/GPU, batching, classifier,
and structured-result support.

The `samples/UnifiedCli` project is a small backend-neutral executable. It
decodes common Windows image formats into BGR24 and demonstrates that changing
from OpenCV DNN to DirectML, OpenVINO, or TensorRT requires no application API
change:

```powershell
dotnet run --project samples/UnifiedCli -- `
  directml C:/models/ppocr/model.json C:/images/test.jpg `
  C:/app/runtimes/win-x64 1 true
```

`samples/UnifiedDemo` is the corresponding WinForms experience program. It
supports backend and GPU selection, manifest browsing, classifier and batch
settings, drag-and-drop images, explicit engine initialization/destruction,
OCR box overlays, structured results, text copying, and stage timings.
