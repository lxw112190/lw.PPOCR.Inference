# Python example

Zero-dependency Python binding for `lw.PPOCR.Inference` via `ctypes`.

## Quick start

```powershell
# copy lw.PPOCR.dll + runtimes/ + models/ into the working directory
pip install opencv-python   # optional, for run_file()
python -c "
from ppocr import OcrEngine
with OcrEngine(backend='opencv', model_manifest='models/ppocrv6-tiny/model.json') as engine:
    result = engine.run_file('models/ppocrv6-tiny/sample.jpg')
    for r in result.regions:
        print(f'{r.text} ({r.score:.4f})')
"
```

## API overview

```python
from ppocr import OcrEngine

# create
engine = OcrEngine(
    backend="directml",          # opencv | directml | openvino | tensorrt
    model_manifest="models/ppocrv6-tiny/model.json",
    device_id=0,
    enable_cls=True,
)

# from a raw BGR24 byte buffer
result = engine.run(pixels, width=640, height=480, stride=1920)

# from a JPEG/PNG file (needs opencv-python)
result = engine.run_file("photo.jpg")

# inspect
for region in result.regions:
    print(f"[{region.text}] score={region.score:.4f}")
    for pt in region.box:
        print(f"  ({pt.x:.1f}, {pt.y:.1f})")
print(f"pipeline: {result.pipeline.total_ms:.2f} ms")

# cleanup (or use `with` statement)
engine.close()
```

## Install as a package

```powershell
pip install -e examples/python/
```

Then `import ppocr` works from any directory.  `lw.PPOCR.dll` and the runtime
tree must still be on the DLL search path.

## Requirements

- Windows 10 / 11 x64
- Python ≥ 3.8
- `lw.PPOCR.dll` and at least one Runtime on the DLL search path
- `opencv-python` (optional, for `run_file`)
