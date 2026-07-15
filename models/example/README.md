# Example Model Package

Place the dictionary and model artifacts beside `model.json`, keeping the names
shown in the manifest, or update the relative paths. Portable Runtimes select
the `onnx` artifact. TensorRT selects `tensorrt` and expects engines generated
for the deployment GPU.

```text
model.json
ppocr_keys_v1.txt
det.onnx
cls.onnx
rec.onnx
det.fp16.engine
cls.fp16.engine
rec.fp16.engine
```

The example classifier shape is the PP-OCRv5 mobile classifier shape. Change it
when a model family uses a different classifier input.
