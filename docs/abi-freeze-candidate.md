# ABI Freeze Candidate

> **Status**: Candidate freeze as of v0.8.0.  These interfaces are **proposed
> for permanent freezing in v1.0.0**.  No breaking changes are planned.  External
> feedback is solicited before the v1.0.0 final freeze.

## Frozen scope (v1 candidate)

### Public C ABI (`include/lw/ppocr/ppocr_api.h`)

| Symbol | Type | Status |
|---|---|---|
| `LW_PPOCR_API_VERSION` (1) | macro | frozen |
| `LW_PPOCR_ABI_FINGERPRINT` | macro | frozen |
| `lw_ppocr_handle` | typedef | frozen |
| `lw_ppocr_status` | enum | frozen — values may only be added |
| `lw_ppocr_backend` | enum | frozen — values may only be added |
| `lw_ppocr_pixel_format` | enum | frozen — values may only be added |
| `lw_ppocr_log_level` | enum | frozen — values may only be added |
| `lw_ppocr_version` | struct | frozen — fields may only be appended |
| `lw_ppocr_config` | struct | frozen — fields may only be appended |
| `lw_ppocr_image` | struct | frozen — fields may only be appended |
| `lw_ppocr_point` | struct | frozen |
| `lw_ppocr_quad` | struct | frozen |
| `lw_ppocr_text_region` | struct | frozen — fields may only be appended |
| `lw_ppocr_timing` | struct | frozen |
| `lw_ppocr_result` | struct | frozen — fields may only be appended |
| `lw_ppocr_capabilities` | struct | frozen — fields may only be appended |
| `lw_ppocr_log_callback` | typedef | frozen |
| `lw_ppocr_get_version` | function | frozen |
| `lw_ppocr_create` | function | frozen |
| `lw_ppocr_run` | function | frozen |
| `lw_ppocr_run_json` | function | frozen |
| `lw_ppocr_result_free` | function | frozen |
| `lw_ppocr_string_free` | function | frozen |
| `lw_ppocr_get_capabilities` | function | frozen |
| `lw_ppocr_get_last_error` | function | frozen |
| `lw_ppocr_destroy` | function | frozen |

### Private Runtime ABI (`src/runtime/include/lw/ppocr/runtime_api.h`)

| Symbol | Type | Status |
|---|---|---|
| `LW_PPOCR_RUNTIME_API_VERSION` (1) | macro | frozen |
| `LW_PPOCR_RUNTIME_ENTRY_NAME` | macro | frozen |
| `lw_ppocr_runtime_api` | struct | frozen — fields may only be appended |
| `lw_ppocr_runtime_get_api_fn` | typedef | frozen |
| `lw_ppocr_runtime_get_api` | function | frozen |

### Model manifest schema

| Symbol | Status |
|---|---|
| `schema_version` 1 | frozen — new optional properties and enum values only |
| `models/model-manifest.schema.json` | canonical schema |

## Not frozen (may change before v1.0.0)

| Area | Rationale |
|---|---|
| Internal `lw_ppocr_engine` struct | Opaque handle; callers never see it |
| Runtime implementation details | Backend-specific; plugin contract covers the boundary |
| .NET binding `OcrOptions` defaults | May adjust defaults before v1.0.0 |
| CLI argument order | May add flags before v1.0.0 |
| `backend_options_json_utf8` format | Backend-specific JSON schema TBD |

## Compatibility rules (proposed for v1.0.0)

1. **No removal.**  A public symbol removed in v1.0.0 stays removed forever —
   this candidate list is therefore exhaustive.
2. **No signature change.**  Parameter types, return types, and calling
   conventions are fixed.
3. **Struct growth only.**  New fields are appended after `reserved_*` padding.
   `struct_size` lets callers detect availability.
4. **Enum growth only.**  New enumerators are appended; existing values persist.
5. **ABI fingerprint.**  Changes only when a new struct field or enumerator is
   added in a way that would change `sizeof` or an existing offset.
6. **Threading contract.**  The documented rules in `ppocr_api.h` are part of
   the ABI commitment.

## Feedback period

This candidate list is open for comment from v0.8.0 through v0.9.0-rc.1.
After v1.0.0 the list is locked.  To propose a change:

- Open a GitHub issue with the tag `abi-freeze`
- Describe the use case that cannot be served without the change
- Include a concrete proposal (new field, new function, new enum value)
