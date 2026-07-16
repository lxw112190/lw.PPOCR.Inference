#pragma once

#include <stdint.h>

#if defined(_WIN32)
#define LW_PPOCR_CALL __cdecl
#if defined(LW_PPOCR_BUILDING_LIBRARY)
#define LW_PPOCR_API __declspec(dllexport)
#else
#define LW_PPOCR_API __declspec(dllimport)
#endif
#else
#define LW_PPOCR_CALL
#define LW_PPOCR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LW_PPOCR_API_VERSION 1u
#define LW_PPOCR_ABI_FINGERPRINT UINT64_C(0x4C5750504F435201)
#define LW_PPOCR_VERSION_MAJOR 1u
#define LW_PPOCR_VERSION_MINOR 0u
#define LW_PPOCR_VERSION_PATCH 0u

typedef struct lw_ppocr_engine* lw_ppocr_handle;

/*
 * Threading contract:
 * - Separate handles are independent and may be used concurrently.
 * - Calls to run/run_json on one handle are safe and may be serialized by the Runtime.
 * - A result or string must be freed with the same handle that created it.
 * - Destroy is not concurrent: finish all calls and free all results before destroy.
 */
typedef int32_t lw_ppocr_status;
typedef int32_t lw_ppocr_backend;
typedef int32_t lw_ppocr_pixel_format;
typedef int32_t lw_ppocr_log_level;

enum {
    LW_PPOCR_STATUS_OK = 0,
    LW_PPOCR_STATUS_INVALID_ARGUMENT = -1,
    LW_PPOCR_STATUS_UNSUPPORTED = -2,
    LW_PPOCR_STATUS_RUNTIME_NOT_FOUND = -3,
    LW_PPOCR_STATUS_RUNTIME_INCOMPATIBLE = -4,
    LW_PPOCR_STATUS_MODEL_ERROR = -5,
    LW_PPOCR_STATUS_INFERENCE_ERROR = -6,
    LW_PPOCR_STATUS_OUT_OF_MEMORY = -7,
    LW_PPOCR_STATUS_INTERNAL_ERROR = -8
};

enum {
    LW_PPOCR_BACKEND_OPENCV_DNN = 1,
    LW_PPOCR_BACKEND_DIRECTML = 2,
    LW_PPOCR_BACKEND_OPENVINO = 3,
    LW_PPOCR_BACKEND_TENSORRT = 4
};

enum {
    LW_PPOCR_PIXEL_FORMAT_GRAY8 = 1,
    LW_PPOCR_PIXEL_FORMAT_BGR24 = 2,
    LW_PPOCR_PIXEL_FORMAT_RGB24 = 3,
    LW_PPOCR_PIXEL_FORMAT_BGRA32 = 4,
    LW_PPOCR_PIXEL_FORMAT_RGBA32 = 5
};

enum {
    LW_PPOCR_LOG_OFF = 0,
    LW_PPOCR_LOG_ERROR = 1,
    LW_PPOCR_LOG_WARNING = 2,
    LW_PPOCR_LOG_INFO = 3,
    LW_PPOCR_LOG_DEBUG = 4
};

typedef void(LW_PPOCR_CALL* lw_ppocr_log_callback)(
    lw_ppocr_log_level level,
    const char* message_utf8,
    void* user_data);

typedef struct lw_ppocr_version {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    const char* product_name_utf8;
    const char* version_utf8;
} lw_ppocr_version;

typedef struct lw_ppocr_config {
    uint32_t struct_size;
    uint32_t api_version;
    lw_ppocr_backend backend;
    int32_t device_id;

    const char* runtime_root_utf8;
    const char* runtime_library_utf8;
    const char* model_manifest_utf8;
    const char* backend_options_json_utf8;

    int32_t enable_cls;
    int32_t limit_side_len;
    float det_db_thresh;
    float det_db_box_thresh;
    float det_db_unclip_ratio;
    int32_t det_use_dilation;
    float cls_threshold;
    int32_t cls_batch_size;
    int32_t rec_batch_size;
    int32_t rec_concurrency;

    lw_ppocr_log_level log_level;
    lw_ppocr_log_callback log_callback;
    void* log_user_data;

    int32_t reserved_i32[8];
    const void* reserved_ptr[4];
} lw_ppocr_config;

typedef struct lw_ppocr_image {
    uint32_t struct_size;
    uint32_t api_version;
    const void* data;
    uint64_t data_size;
    int32_t width;
    int32_t height;
    int32_t stride;
    lw_ppocr_pixel_format pixel_format;
    int32_t reserved_i32[4];
} lw_ppocr_image;

typedef struct lw_ppocr_point {
    float x;
    float y;
} lw_ppocr_point;

typedef struct lw_ppocr_quad {
    lw_ppocr_point top_left;
    lw_ppocr_point top_right;
    lw_ppocr_point bottom_right;
    lw_ppocr_point bottom_left;
} lw_ppocr_quad;

typedef struct lw_ppocr_text_region {
    lw_ppocr_quad box;
    const char* text_utf8;
    float score;
    int32_t cls_label;
    float cls_score;
    int32_t reserved_i32[4];
} lw_ppocr_text_region;

typedef struct lw_ppocr_timing {
    double preprocess_ms;
    double inference_ms;
    double postprocess_ms;
    double total_ms;
} lw_ppocr_timing;

typedef struct lw_ppocr_result {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t region_count;
    const lw_ppocr_text_region* regions;
    lw_ppocr_timing detector;
    lw_ppocr_timing classifier;
    lw_ppocr_timing recognizer;
    lw_ppocr_timing pipeline;
    int32_t reserved_i32[8];
    const void* reserved_ptr[4];
} lw_ppocr_result;

typedef struct lw_ppocr_capabilities {
    uint32_t struct_size;
    uint32_t api_version;
    lw_ppocr_backend backend;
    const char* backend_name_utf8;
    int32_t supports_cpu;
    int32_t supports_gpu;
    int32_t supports_fp16;
    int32_t supports_int8;
    int32_t supports_cls;
    int32_t reserved_i32[8];
} lw_ppocr_capabilities;

LW_PPOCR_API lw_ppocr_status LW_PPOCR_CALL lw_ppocr_get_version(
    lw_ppocr_version* version);

LW_PPOCR_API lw_ppocr_status LW_PPOCR_CALL lw_ppocr_create(
    const lw_ppocr_config* config,
    lw_ppocr_handle* handle);

LW_PPOCR_API lw_ppocr_status LW_PPOCR_CALL lw_ppocr_run(
    lw_ppocr_handle handle,
    const lw_ppocr_image* image,
    lw_ppocr_result** result);

LW_PPOCR_API lw_ppocr_status LW_PPOCR_CALL lw_ppocr_run_json(
    lw_ppocr_handle handle,
    const lw_ppocr_image* image,
    char** json_utf8,
    uint64_t* json_length);

LW_PPOCR_API void LW_PPOCR_CALL lw_ppocr_result_free(
    lw_ppocr_handle handle,
    lw_ppocr_result* result);

LW_PPOCR_API void LW_PPOCR_CALL lw_ppocr_string_free(
    lw_ppocr_handle handle,
    char* value);

LW_PPOCR_API lw_ppocr_status LW_PPOCR_CALL lw_ppocr_get_capabilities(
    lw_ppocr_handle handle,
    lw_ppocr_capabilities* capabilities);

LW_PPOCR_API uint64_t LW_PPOCR_CALL lw_ppocr_get_last_error(
    lw_ppocr_handle handle,
    char* buffer_utf8,
    uint64_t buffer_capacity);

LW_PPOCR_API void LW_PPOCR_CALL lw_ppocr_destroy(
    lw_ppocr_handle* handle);

#ifdef __cplusplus
}
#endif
