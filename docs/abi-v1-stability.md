# ABI v1 Stability Guarantee

> **Effective**: lw.PPOCR.Inference v1.0.0
> **Scope**: Public C ABI, private Runtime ABI, model manifest schema v1

## What is frozen

### Public C ABI (`include/lw/ppocr/ppocr_api.h`)

All types, enums, functions, and macros documented in
[abi-freeze-candidate.md](abi-freeze-candidate.md) are permanently frozen
as of v1.0.0.  Specifically:

- `LW_PPOCR_API_VERSION` = 1 (permanent)
- `LW_PPOCR_ABI_FINGERPRINT_V1` = `0x4C5750504F435201` (stable base;
  the low byte increments on compatible additions only)
- `LW_PPOCR_*_V1_SIZE` macros define the frozen minimum prefix size accepted
  by v1 Loaders and Runtimes
- All `lw_ppocr_*` structs: fields may be **appended** after `reserved_*`
  padding; existing fields are never removed, renamed, or retyped
- All `lw_ppocr_*` enums: values may be **appended**; existing values persist
- All `lw_ppocr_*` functions: signatures are permanent; functions are never
  removed
- `lw_ppocr_log_callback` typedef: permanent
- Threading and memory ownership contract: permanent

### Private Runtime ABI (`src/runtime/include/lw/ppocr/runtime_api.h`)

- `LW_PPOCR_RUNTIME_API_VERSION` = 1 (permanent)
- `LW_PPOCR_RUNTIME_ENTRY_NAME` = `"lw_ppocr_runtime_get_api"` (permanent)
- `lw_ppocr_runtime_api` struct: append-only, same rules as public structs
- `lw_ppocr_runtime_get_api_fn` typedef: permanent

### Model manifest schema

- Schema v1 fields and types are frozen
- Future schema versions (2, 3, …) coexist with v1
- Applications specify `schema_version` in each manifest

## How we evolve without breaking

### Adding a new struct field

```c
// Before (v1.0.0):
typedef struct lw_ppocr_config {
    // ... existing fields ...
    int32_t reserved_i32[8];
    const void* reserved_ptr[4];
} lw_ppocr_config;

// After (v1.1.0):
typedef struct lw_ppocr_config {
    // ... existing fields (unchanged) ...
    int32_t reserved_i32[8];
    const void* reserved_ptr[4];
    int32_t new_field;          // ← appended after the frozen v1 prefix
} lw_ppocr_config;
```

Callers compiled against v1.0.0 headers set `struct_size = sizeof(v1.0.0 struct)`.
The Runtime checks `struct_size >= LW_PPOCR_CONFIG_V1_SIZE`, reads `new_field`
only when `struct_size` reaches the end of that field, and otherwise uses the
documented legacy default. This is the standard Windows `cbSize` pattern.
Callers must zero-initialize structures so reserved storage remains available
for compatible evolution.

### Adding a new enum value

```c
enum {
    LW_PPOCR_BACKEND_OPENCV_DNN = 1,
    LW_PPOCR_BACKEND_DIRECTML   = 2,
    LW_PPOCR_BACKEND_OPENVINO   = 3,
    LW_PPOCR_BACKEND_TENSORRT   = 4,
    LW_PPOCR_BACKEND_ONNXRUNTIME = 5, // appended without renumbering v1 values
};
```

Existing callers never see the new value; they pass values 1–4 which work
unchanged.  New callers can pass 5.

### Adding a new function

New functions are added with new names (e.g., `lw_ppocr_run_async`).  Existing
functions are never removed or changed.  Callers link against the same DLL and
use `GetProcAddress` for optional functions.

### v1.1.0 compatible extension

v1.1.0 is the first application of these rules. It adds
`lw_ppocr_recognize`, `lw_ppocr_recognize_batch`, recognition result types,
and two Runtime function-table entries at the tail. `LW_PPOCR_API_VERSION`
remains 1. A v1.1 Loader accepts a v1.0 Runtime for the original OCR calls and
returns `LW_PPOCR_STATUS_UNSUPPORTED` when recognition-only inference is not
present. A v1.0 Loader continues to use the frozen v1 prefix of a v1.1 Runtime.

## What is NOT frozen

| Area | May change |
|---|---|
| Backend Runtime implementations | Performance, bug fixes, internal refactoring |
| Packaging layout | ZIP structure, directory naming |
| .NET binding API | May evolve independently |
| CLI and Demo UI | May change |
| Configuration defaults | `rec_concurrency` etc. may tune over time |
| `backend_options_json_utf8` format | Backend-specific and may expand |

## Security support

- **Critical vulnerabilities**: Patches backported for ≥ 2 years from v1.0.0
- **Report**: Open a GitHub issue or email the maintainer (see README)
- **Scope**: Loader, Core, and all four production Runtimes

## Questions

**Q: Will there be an ABI v2?**
Not unless a fundamental redesign is required that cannot be accomplished
through append-only additions.  If an ABI v2 ever ships, it will coexist
with v1 for a transition period.

**Q: How do I check the ABI version at runtime?**
```c
lw_ppocr_version v = { .struct_size = sizeof(v) };
lw_ppocr_get_version(&v);
if (v.api_version < 1) { /* unsupported */ }
```

**Q: Does a new ABI fingerprint mean a breaking change?**
No. The low byte increments on compatible additions (new fields, new
enumerators). The Loader accepts fingerprints in the frozen
`LW_PPOCR_ABI_FINGERPRINT_FAMILY` and validates the v1 minimum prefix sizes,
so a newer append-only Runtime remains usable through the v1 Loader contract.
