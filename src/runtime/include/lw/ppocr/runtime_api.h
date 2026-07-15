#pragma once

#include <lw/ppocr/ppocr_api.h>

#include <stdint.h>

#if defined(_WIN32) && defined(LW_PPOCR_RUNTIME_BUILDING_LIBRARY)
#define LW_PPOCR_RUNTIME_EXPORT __declspec(dllexport)
#else
#define LW_PPOCR_RUNTIME_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LW_PPOCR_RUNTIME_API_VERSION 1u
#define LW_PPOCR_RUNTIME_ENTRY_NAME "lw_ppocr_runtime_get_api"

typedef struct lw_ppocr_runtime_api {
    uint32_t struct_size;
    uint32_t api_version;
    const char* runtime_name_utf8;

    lw_ppocr_status(LW_PPOCR_CALL* create)(
        const lw_ppocr_config* config,
        void** runtime_handle);

    lw_ppocr_status(LW_PPOCR_CALL* run)(
        void* runtime_handle,
        const lw_ppocr_image* image,
        lw_ppocr_result** result);

    lw_ppocr_status(LW_PPOCR_CALL* run_json)(
        void* runtime_handle,
        const lw_ppocr_image* image,
        char** json_utf8,
        uint64_t* json_length);

    void(LW_PPOCR_CALL* result_free)(
        void* runtime_handle,
        lw_ppocr_result* result);

    void(LW_PPOCR_CALL* string_free)(
        void* runtime_handle,
        char* value);

    lw_ppocr_status(LW_PPOCR_CALL* get_capabilities)(
        void* runtime_handle,
        lw_ppocr_capabilities* capabilities);

    uint64_t(LW_PPOCR_CALL* get_last_error)(
        void* runtime_handle,
        char* buffer_utf8,
        uint64_t buffer_capacity);

    void(LW_PPOCR_CALL* destroy)(void** runtime_handle);

    uint32_t public_config_size;
    uint32_t public_image_size;
    uint32_t public_result_size;
    uint32_t public_capabilities_size;
    uint64_t public_abi_fingerprint;
} lw_ppocr_runtime_api;

typedef lw_ppocr_status(LW_PPOCR_CALL* lw_ppocr_runtime_get_api_fn)(
    uint32_t requested_api_version,
    const lw_ppocr_runtime_api** runtime_api);

LW_PPOCR_RUNTIME_EXPORT lw_ppocr_status LW_PPOCR_CALL lw_ppocr_runtime_get_api(
    uint32_t requested_api_version,
    const lw_ppocr_runtime_api** runtime_api);

#ifdef __cplusplus
}
#endif
