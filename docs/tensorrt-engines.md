# TensorRT Engine Preparation

The TensorRT Runtime loads serialized engines directly. Replace `<TRTEXEC>`,
ONNX paths, and output paths with locations on the target computer.

## FP16 conversion

Detector profile:

```powershell
& "<TRTEXEC>" `
  --onnx="detector.onnx" `
  --saveEngine="detector_fp16.engine" `
  --fp16 `
  --minShapes=x:1x3x32x32 `
  --optShapes=x:1x3x960x960 `
  --maxShapes=x:1x3x960x960
```

Recognizer profile for batches up to 16 and widths up to 1280:

```powershell
& "<TRTEXEC>" `
  --onnx="recognizer.onnx" `
  --saveEngine="recognizer_fp16.engine" `
  --fp16 `
  --minShapes=x:1x3x48x32 `
  --optShapes=x:4x3x48x320 `
  --maxShapes=x:16x3x48x1280
```

Direction classifier profile:

```powershell
& "<TRTEXEC>" `
  --onnx="classifier.onnx" `
  --saveEngine="classifier_fp16.engine" `
  --fp16 `
  --minShapes=x:1x3x80x160 `
  --optShapes=x:8x3x80x160 `
  --maxShapes=x:16x3x80x160
```

The tensor name in these examples is `x`. Use the actual ONNX input name when
it differs. Keep `rec_batch_size` and recognition widths inside the generated
profile. TensorRT engines should be generated on the deployment computer, or on
an identical GPU, driver, CUDA, and TensorRT stack.

At runtime the recognizer reads the maximum width from the engine optimization
profile. Text crops wider than that profile are resized to the declared maximum
instead of being passed to `setInputShape` with an invalid dimension. A wider
profile can therefore be deployed without recompiling the Runtime.

## Model manifest

Each stage can contain both portable ONNX and machine-specific TensorRT
artifacts. The TensorRT Runtime selects the `tensorrt` entry.

```json
{
  "input_shape": [1, 3, 48, 320],
  "artifacts": {
    "onnx": {
      "path": "recognizer.onnx",
      "format": "onnx",
      "precision": "fp32"
    },
    "tensorrt": {
      "path": "recognizer_fp16.engine",
      "format": "tensorrt-engine",
      "precision": "fp16"
    }
  }
}
```
