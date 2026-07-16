/*
 * lw.PPOCR.Inference — minimal C example
 *
 * Build (MSVC x64 Native Tools Command Prompt):
 *   cl /utf-8 /Fe:ppocr_example.exe main.c /link lw.PPOCR.lib
 *
 * Or link dynamically at runtime (no import lib needed):
 *   cl /utf-8 /Fe:ppocr_example.exe main.c
 *
 * The example demonstrates the full C ABI lifecycle without any
 * external dependencies beyond the Windows SDK and lw.PPOCR.dll.
 */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- public API types (mirrored from include/lw/ppocr/ppocr_api.h) ----- */

#define LW_PPOCR_API_VERSION 1u

typedef struct lw_ppocr_engine lw_ppocr_engine;
typedef lw_ppocr_engine* lw_ppocr_handle;
typedef int32_t lw_ppocr_status;
typedef int32_t lw_ppocr_backend;
typedef int32_t lw_ppocr_pixel_format;
typedef int32_t lw_ppocr_log_level;

enum {
    LW_PPOCR_STATUS_OK                =  0,
    LW_PPOCR_STATUS_INVALID_ARGUMENT  = -1,
    LW_PPOCR_STATUS_RUNTIME_NOT_FOUND = -3,
};

enum {
    LW_PPOCR_BACKEND_OPENCV_DNN = 1,
    LW_PPOCR_BACKEND_DIRECTML   = 2,
    LW_PPOCR_BACKEND_OPENVINO   = 3,
    LW_PPOCR_BACKEND_TENSORRT   = 4,
    LW_PPOCR_BACKEND_ONNXRUNTIME = 5,
};

enum {
    LW_PPOCR_PIXEL_FORMAT_BGR24  = 2,
};

typedef struct lw_ppocr_point {
    float x, y;
} lw_ppocr_point;

typedef struct lw_ppocr_quad {
    lw_ppocr_point top_left, top_right, bottom_right, bottom_left;
} lw_ppocr_quad;

typedef struct lw_ppocr_text_region {
    lw_ppocr_quad box;
    const char* text_utf8;
    float       score;
    int32_t     cls_label;
    float       cls_score;
    int32_t     reserved_i32[4];
} lw_ppocr_text_region;

typedef struct lw_ppocr_timing {
    double preprocess_ms;
    double inference_ms;
    double postprocess_ms;
    double total_ms;
} lw_ppocr_timing;

typedef struct lw_ppocr_result {
    uint32_t                  struct_size;
    uint32_t                  api_version;
    uint64_t                  region_count;
    const lw_ppocr_text_region* regions;
    lw_ppocr_timing           detector;
    lw_ppocr_timing           classifier;
    lw_ppocr_timing           recognizer;
    lw_ppocr_timing           pipeline;
    int32_t                   reserved_i32[8];
    const void*               reserved_ptr[4];
} lw_ppocr_result;

typedef struct lw_ppocr_recognition {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t source_index;
    const char* text_utf8;
    float score;
    int32_t cls_label;
    float cls_score;
    int32_t reserved_i32[4];
    const void* reserved_ptr[2];
} lw_ppocr_recognition;

typedef struct lw_ppocr_recognition_result {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t item_count;
    const lw_ppocr_recognition* items;
    lw_ppocr_timing classifier;
    lw_ppocr_timing recognizer;
    lw_ppocr_timing pipeline;
    int32_t reserved_i32[8];
    const void* reserved_ptr[4];
} lw_ppocr_recognition_result;

typedef struct lw_ppocr_image {
    uint32_t              struct_size;
    uint32_t              api_version;
    const void*           data;
    uint64_t              data_size;
    int32_t               width;
    int32_t               height;
    int32_t               stride;
    lw_ppocr_pixel_format pixel_format;
    int32_t               reserved_i32[4];
} lw_ppocr_image;

typedef struct lw_ppocr_config {
    uint32_t            struct_size;
    uint32_t            api_version;
    lw_ppocr_backend    backend;
    int32_t             device_id;
    const char*         runtime_root_utf8;
    const char*         runtime_library_utf8;
    const char*         model_manifest_utf8;
    const char*         backend_options_json_utf8;
    int32_t             enable_cls;
    int32_t             limit_side_len;
    float               det_db_thresh;
    float               det_db_box_thresh;
    float               det_db_unclip_ratio;
    int32_t             det_use_dilation;
    float               cls_threshold;
    int32_t             cls_batch_size;
    int32_t             rec_batch_size;
    int32_t             rec_concurrency;
    lw_ppocr_log_level  log_level;
    void*               log_callback;
    void*               log_user_data;
    int32_t             reserved_i32[8];
    const void*         reserved_ptr[4];
} lw_ppocr_config;

/* ----- function pointer types ----- */

typedef lw_ppocr_status (__cdecl *lw_ppocr_create_fn)(
    const lw_ppocr_config* config, lw_ppocr_handle* handle);
typedef lw_ppocr_status (__cdecl *lw_ppocr_run_fn)(
    lw_ppocr_handle handle, const lw_ppocr_image* image,
    lw_ppocr_result** result);
typedef void (__cdecl *lw_ppocr_result_free_fn)(
    lw_ppocr_handle handle, lw_ppocr_result* result);
typedef lw_ppocr_status (__cdecl *lw_ppocr_recognize_batch_fn)(
    lw_ppocr_handle handle, const lw_ppocr_image* images,
    uint64_t image_count, lw_ppocr_recognition_result** result);
typedef void (__cdecl *lw_ppocr_recognition_result_free_fn)(
    lw_ppocr_handle handle, lw_ppocr_recognition_result* result);
typedef void (__cdecl *lw_ppocr_destroy_fn)(lw_ppocr_handle* handle);
typedef lw_ppocr_status (__cdecl *lw_ppocr_get_capabilities_fn)(
    lw_ppocr_handle handle, void* capabilities);
typedef uint64_t (__cdecl *lw_ppocr_get_last_error_fn)(
    lw_ppocr_handle handle, char* buffer_utf8, uint64_t capacity);

/* ----- helper: load function pointer from DLL ----- */

static HMODULE g_dll = NULL;

static void* load_fn(const char* name) {
    void* fn = GetProcAddress(g_dll, name);
    if (fn == NULL) {
        fprintf(stderr, "error: %s not found in lw.PPOCR.dll\n", name);
        exit(1);
    }
    return fn;
}

/* ----- helper: print last error ----- */

static void print_last_error(lw_ppocr_get_last_error_fn get_error,
                              lw_ppocr_handle handle) {
    uint64_t needed = get_error(handle, NULL, 0);
    if (needed <= 1) return;
    char* buffer = (char*)malloc((size_t)needed);
    if (buffer == NULL) return;
    get_error(handle, buffer, needed);
    fprintf(stderr, "  native error: %s\n", buffer);
    free(buffer);
}

/* ----- main ----- */

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    /* 1. load the DLL */
    g_dll = LoadLibraryA("lw.PPOCR.dll");
    if (g_dll == NULL) {
        fprintf(stderr, "error: cannot load lw.PPOCR.dll (is it in the PATH?)\n");
        return 1;
    }

    lw_ppocr_create_fn          create_fn  = (lw_ppocr_create_fn)load_fn("lw_ppocr_create");
    lw_ppocr_run_fn             run_fn     = (lw_ppocr_run_fn)load_fn("lw_ppocr_run");
    lw_ppocr_result_free_fn     free_fn    = (lw_ppocr_result_free_fn)load_fn("lw_ppocr_result_free");
    lw_ppocr_recognize_batch_fn recognize_fn =
        (lw_ppocr_recognize_batch_fn)load_fn("lw_ppocr_recognize_batch");
    lw_ppocr_recognition_result_free_fn recognition_free_fn =
        (lw_ppocr_recognition_result_free_fn)load_fn(
            "lw_ppocr_recognition_result_free");
    lw_ppocr_destroy_fn         destroy_fn = (lw_ppocr_destroy_fn)load_fn("lw_ppocr_destroy");
    lw_ppocr_get_last_error_fn  error_fn   = (lw_ppocr_get_last_error_fn)load_fn("lw_ppocr_get_last_error");

    /* 2. create engine */
    lw_ppocr_config config = {0};
    config.struct_size        = sizeof(config);
    config.api_version        = LW_PPOCR_API_VERSION;
    config.backend            = LW_PPOCR_BACKEND_OPENCV_DNN;
    config.device_id          = 0;
    config.model_manifest_utf8 = "models/ppocrv6-tiny/model.json";
    config.enable_cls         = 1;
    config.limit_side_len     = 960;
    config.det_db_thresh      = 0.3f;
    config.det_db_box_thresh  = 0.6f;
    config.det_db_unclip_ratio = 1.6f;
    config.cls_threshold      = 0.9f;
    config.cls_batch_size     = 8;
    config.rec_batch_size     = 1;
    config.rec_concurrency    = 2;

    lw_ppocr_handle engine = NULL;
    lw_ppocr_status status = create_fn(&config, &engine);
    if (status != LW_PPOCR_STATUS_OK || engine == NULL) {
        fprintf(stderr, "error: lw_ppocr_create failed (status=%d)\n", status);
        print_last_error(error_fn, engine);
        FreeLibrary(g_dll);
        return 1;
    }
    printf("engine created\n");

    /* 3. prepare a small test image (2×2 BGR, black pixels) */
    unsigned char pixels[12] = {0};  /* 2×2×3 = 12 bytes */
    lw_ppocr_image image = {0};
    image.struct_size  = sizeof(image);
    image.api_version  = LW_PPOCR_API_VERSION;
    image.data         = pixels;
    image.data_size    = sizeof(pixels);
    image.width        = 2;
    image.height       = 2;
    image.stride       = 6;   /* width × 3 */
    image.pixel_format = LW_PPOCR_PIXEL_FORMAT_BGR24;

    /* 4. run OCR */
    lw_ppocr_result* result = NULL;
    status = run_fn(engine, &image, &result);
    if (status != LW_PPOCR_STATUS_OK || result == NULL) {
        fprintf(stderr, "error: lw_ppocr_run failed (status=%d)\n", status);
        print_last_error(error_fn, engine);
        destroy_fn(&engine);
        FreeLibrary(g_dll);
        return 1;
    }

    /* 5. print results */
    printf("regions: %llu\n", (unsigned long long)result->region_count);
    for (uint64_t i = 0; i < result->region_count; i++) {
        const lw_ppocr_text_region* r = &result->regions[i];
        printf("  [%llu] \"%s\" score=%.4f\n",
               (unsigned long long)i,
               r->text_utf8 != NULL ? r->text_utf8 : "",
               r->score);
    }
    printf("detector:  %.2f ms\n", result->detector.total_ms);
    printf("classifier: %.2f ms\n", result->classifier.total_ms);
    printf("recognizer: %.2f ms\n", result->recognizer.total_ms);
    printf("pipeline:  %.2f ms\n", result->pipeline.total_ms);

    /* 6. recognition-only: image is already a cropped text-line image */
    free_fn(engine, result);
    {
        lw_ppocr_image crops[2] = {image, image};
        lw_ppocr_recognition_result* recognition = NULL;
        status = recognize_fn(engine, crops, 2, &recognition);
        if (status != LW_PPOCR_STATUS_OK || recognition == NULL) {
            fprintf(stderr, "error: recognition-only failed (status=%d)\n", status);
            print_last_error(error_fn, engine);
            destroy_fn(&engine);
            FreeLibrary(g_dll);
            return 1;
        }
        for (uint64_t i = 0; i < recognition->item_count; i++) {
            printf("crop[%llu]: \"%s\" score=%.4f\n",
                (unsigned long long)recognition->items[i].source_index,
                recognition->items[i].text_utf8 != NULL
                    ? recognition->items[i].text_utf8 : "",
                recognition->items[i].score);
        }
        printf("recognition-only: %.2f ms\n",
            recognition->pipeline.total_ms);
        recognition_free_fn(engine, recognition);
    }

    /* 7. cleanup */
    destroy_fn(&engine);
    if (engine != NULL) {
        fprintf(stderr, "error: destroy did not clear handle\n");
    }
    FreeLibrary(g_dll);
    printf("done — C ABI example completed successfully\n");
    return 0;
}
