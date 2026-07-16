# Model Manifest

`model.json` is the single source of truth for an OCR model package. Applications select a manifest and a backend; they must not hard-code model filenames, dictionary paths, or tensor shapes.

## Schema

The canonical JSON Schema is at `models/model-manifest.schema.json`.

- **Schema version**: 1
- **Status**: **Frozen** as of lw.PPOCR.Inference v0.3.0
- **Compatibility promise**: Future schema versions (2, 3, …) may add new optional properties and new enum values, but will not remove, rename, or retype fields defined in v1.

The schema `$id` is:

```text
https://raw.githubusercontent.com/lxw112190/lw.PPOCR.Inference/main/models/model-manifest.schema.json
```

## Required fields

| Field | Type | Description |
|---|---|---|
| `schema_version` | integer (const 1) | Manifest schema version. |
| `name` | string | Human-readable model package name. |
| `family` | string | Model family identifier (e.g. `"PP-OCRv6"`). |
| `dictionary` | object (`file`) | Recognition dictionary file reference. |
| `stages` | object | Pipeline stage definitions. |

`stages` must contain `detector` and `recognizer`. `classifier` is optional.

## Optional fields

| Field | Type | Default | Description |
|---|---|---|---|
| `language` | string | `"ch"` | Language code for the dictionary. |

## `file` object

| Field | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | Relative path from the manifest directory. |
| `sha256` | string (64 hex) | no | SHA-256 digest of the file. |

## `stage` object

| Field | Type | Required | Description |
|---|---|---|---|
| `input_shape` | integer array (3–4) | yes | Expected input tensor shape. `-1` for dynamic dims. |
| `artifacts` | object (map) | yes | Backend-specific artifact map (≥1 entry). |
| `input_name` | string | no | Input tensor name in the model graph. |
| `output_name` | string | no | Output tensor name in the model graph. |

## `artifact` object

| Field | Type | Required | Description |
|---|---|---|---|
| `path` | string | yes | Relative path from the manifest directory. |
| `format` | enum | yes | `"onnx"`, `"openvino-ir"`, or `"tensorrt-engine"`. |
| `precision` | enum | yes | `"fp32"`, `"fp16"`, or `"int8"`. |
| `sha256` | string (64 hex) | no | SHA-256 digest of the artifact. |

## Artifact keys

The `artifacts` map inside each stage uses local keys to identify backends:

| Key | Used by | Typical format |
|---|---|---|
| `onnx` | OpenCV DNN, DirectML, OpenVINO | `"onnx"`, `"fp32"` |
| `tensorrt` | TensorRT | `"tensorrt-engine"`, `"fp16"` |

Additional keys (e.g. `openvino`) can be added without changing the schema. The schema only validates that every value conforms to the `artifact` definition — it does not restrict the map keys.

## Example

```json
{
  "schema_version": 1,
  "name": "PP-OCRv6 tiny Chinese",
  "family": "PP-OCRv6",
  "language": "ch",
  "dictionary": {
    "path": "ppocr_keys.txt"
  },
  "stages": {
    "detector": {
      "input_name": "x",
      "input_shape": [-1, 3, -1, -1],
      "artifacts": {
        "onnx": {
          "path": "det.onnx",
          "format": "onnx",
          "precision": "fp32"
        },
        "tensorrt": {
          "path": "det.fp16.engine",
          "format": "tensorrt-engine",
          "precision": "fp16"
        }
      }
    },
    "classifier": {
      "input_shape": [1, 3, 80, 160],
      "artifacts": {
        "onnx": {
          "path": "cls.onnx",
          "format": "onnx",
          "precision": "fp32"
        }
      }
    },
    "recognizer": {
      "input_shape": [1, 3, 48, 320],
      "artifacts": {
        "onnx": {
          "path": "rec.onnx",
          "format": "onnx",
          "precision": "fp32"
        },
        "tensorrt": {
          "path": "rec.fp16.engine",
          "format": "tensorrt-engine",
          "precision": "fp16"
        }
      }
    }
  }
}
```

## Backward compatibility

1. **Schema v1 fields are frozen.** New optional properties may be added. Existing required fields will stay required. Types and enum values will not narrow.
2. **Runtime ignores unknown properties.** A manifest written for schema v2 will still load in a v1-aware Runtime, though the Runtime may ignore new fields.
3. **Artifact keys are open-ended.** Adding a key like `"openvino"` does not require a schema update.
4. **Coordinate systems** are top-left origin in image pixels, consistent with OpenCV conventions.
