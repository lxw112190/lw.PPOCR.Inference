#include <lw/ppocr/runtime_api.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

struct StubEngine {
    lw_ppocr_backend backend = LW_PPOCR_BACKEND_OPENCV_DNN;
    lw_ppocr_log_level log_level = LW_PPOCR_LOG_OFF;
    lw_ppocr_log_callback log_callback = nullptr;
    void* log_user_data = nullptr;
};

struct StubRecognitionResult {
    lw_ppocr_recognition_result value{};
    std::vector<lw_ppocr_recognition> items;
};

thread_local std::string g_last_error;

uint64_t CopyText(const std::string& text, char* buffer, uint64_t capacity) {
    const uint64_t required = static_cast<uint64_t>(text.size()) + 1;
    if (buffer != nullptr && capacity > 0) {
        const size_t count = static_cast<size_t>(
            std::min<uint64_t>(capacity - 1, static_cast<uint64_t>(text.size())));
        if (count > 0) {
            std::memcpy(buffer, text.data(), count);
        }
        buffer[count] = '\0';
    }
    return required;
}

lw_ppocr_status LW_PPOCR_CALL Create(
    const lw_ppocr_config* config,
    void** runtime_handle) {
    if (config == nullptr || runtime_handle == nullptr) {
        g_last_error = "Stub Runtime received an invalid create request";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *runtime_handle = nullptr;

    auto* engine = new (std::nothrow) StubEngine();
    if (engine == nullptr) {
        g_last_error = "Stub Runtime failed to allocate an engine";
        return LW_PPOCR_STATUS_OUT_OF_MEMORY;
    }

    engine->backend = config->backend;
    engine->log_level = config->log_level;
    engine->log_callback = config->log_callback;
    engine->log_user_data = config->log_user_data;
    *runtime_handle = engine;
    g_last_error.clear();

    if (engine->log_callback != nullptr && engine->log_level >= LW_PPOCR_LOG_INFO) {
        engine->log_callback(LW_PPOCR_LOG_INFO,
            "Stub Runtime engine created", engine->log_user_data);
    }
    return LW_PPOCR_STATUS_OK;
}

lw_ppocr_status LW_PPOCR_CALL Run(
    void* runtime_handle,
    const lw_ppocr_image*,
    lw_ppocr_result** result) {
    if (runtime_handle == nullptr || result == nullptr) {
        g_last_error = "Stub Runtime received an invalid run request";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *result = static_cast<lw_ppocr_result*>(std::calloc(1, sizeof(lw_ppocr_result)));
    if (*result == nullptr) {
        g_last_error = "Stub Runtime failed to allocate a result";
        return LW_PPOCR_STATUS_OUT_OF_MEMORY;
    }

    (*result)->struct_size = sizeof(lw_ppocr_result);
    (*result)->api_version = LW_PPOCR_API_VERSION;
    g_last_error.clear();
    return LW_PPOCR_STATUS_OK;
}

lw_ppocr_status LW_PPOCR_CALL RunJson(
    void* runtime_handle,
    const lw_ppocr_image*,
    char** json_utf8,
    uint64_t* json_length) {
    if (runtime_handle == nullptr || json_utf8 == nullptr || json_length == nullptr) {
        g_last_error = "Stub Runtime received invalid JSON outputs";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }

    *json_utf8 = static_cast<char*>(std::malloc(3));
    if (*json_utf8 == nullptr) {
        g_last_error = "Stub Runtime failed to allocate JSON output";
        return LW_PPOCR_STATUS_OUT_OF_MEMORY;
    }
    std::memcpy(*json_utf8, "[]", 3);
    *json_length = 2;
    g_last_error.clear();
    return LW_PPOCR_STATUS_OK;
}

void LW_PPOCR_CALL ResultFree(void*, lw_ppocr_result* result) {
    std::free(result);
}

lw_ppocr_status LW_PPOCR_CALL RecognizeBatch(
    void* runtime_handle, const lw_ppocr_image*, uint64_t image_count,
    lw_ppocr_recognition_result** result) {
    if (runtime_handle == nullptr || image_count == 0 || result == nullptr) {
        g_last_error = "Stub Runtime received an invalid recognition request";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *result = nullptr;
    auto* owned = new (std::nothrow) StubRecognitionResult();
    if (owned == nullptr) {
        g_last_error = "Stub Runtime failed to allocate a recognition result";
        return LW_PPOCR_STATUS_OUT_OF_MEMORY;
    }
    try {
        owned->items.resize(static_cast<size_t>(image_count));
    } catch (...) {
        delete owned;
        g_last_error = "Stub Runtime failed to allocate recognition items";
        return LW_PPOCR_STATUS_OUT_OF_MEMORY;
    }
    for (size_t index = 0; index < owned->items.size(); ++index) {
        owned->items[index].struct_size = sizeof(lw_ppocr_recognition);
        owned->items[index].api_version = LW_PPOCR_API_VERSION;
        owned->items[index].source_index = static_cast<uint64_t>(index);
        owned->items[index].text_utf8 = "";
        owned->items[index].cls_label = -1;
    }
    owned->value.struct_size = sizeof(lw_ppocr_recognition_result);
    owned->value.api_version = LW_PPOCR_API_VERSION;
    owned->value.item_count = image_count;
    owned->value.items = owned->items.data();
    owned->value.reserved_ptr[0] = owned;
    *result = &owned->value;
    g_last_error.clear();
    return LW_PPOCR_STATUS_OK;
}

void LW_PPOCR_CALL RecognitionResultFree(
    void*, lw_ppocr_recognition_result* result) {
    if (result != nullptr) {
        delete static_cast<StubRecognitionResult*>(
            const_cast<void*>(result->reserved_ptr[0]));
    }
}

void LW_PPOCR_CALL StringFree(void*, char* value) {
    std::free(value);
}

lw_ppocr_status LW_PPOCR_CALL GetCapabilities(
    void* runtime_handle,
    lw_ppocr_capabilities* capabilities) {
    if (runtime_handle == nullptr || capabilities == nullptr ||
        capabilities->struct_size < LW_PPOCR_CAPABILITIES_V1_SIZE) {
        g_last_error = "Stub Runtime received invalid capabilities storage";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }

    const auto* engine = static_cast<const StubEngine*>(runtime_handle);
    capabilities->api_version = LW_PPOCR_API_VERSION;
    capabilities->backend = engine->backend;
    capabilities->backend_name_utf8 = "Contract Test Stub";
    capabilities->supports_cpu = 1;
    capabilities->supports_gpu = 0;
    capabilities->supports_fp16 = 0;
    capabilities->supports_int8 = 0;
    capabilities->supports_cls = 1;
    g_last_error.clear();
    return LW_PPOCR_STATUS_OK;
}

uint64_t LW_PPOCR_CALL GetLastError(
    void*, char* buffer_utf8, uint64_t buffer_capacity) {
    return CopyText(g_last_error, buffer_utf8, buffer_capacity);
}

void LW_PPOCR_CALL Destroy(void** runtime_handle) {
    if (runtime_handle == nullptr || *runtime_handle == nullptr) {
        return;
    }
    delete static_cast<StubEngine*>(*runtime_handle);
    *runtime_handle = nullptr;
    g_last_error.clear();
}

#if defined(LW_PPOCR_STUB_FORWARD_COMPAT)
constexpr uint32_t kAdvertisedRuntimeApiSize =
    static_cast<uint32_t>(sizeof(lw_ppocr_runtime_api) + 16);
constexpr uint32_t kAdvertisedConfigSize =
    static_cast<uint32_t>(sizeof(lw_ppocr_config) + 16);
constexpr uint32_t kAdvertisedImageSize =
    static_cast<uint32_t>(sizeof(lw_ppocr_image) + 16);
constexpr uint32_t kAdvertisedResultSize =
    static_cast<uint32_t>(sizeof(lw_ppocr_result) + 16);
constexpr uint32_t kAdvertisedCapabilitiesSize =
    static_cast<uint32_t>(sizeof(lw_ppocr_capabilities) + 16);
constexpr uint64_t kAdvertisedFingerprint = LW_PPOCR_ABI_FINGERPRINT + 1;
#else
constexpr uint32_t kAdvertisedRuntimeApiSize = sizeof(lw_ppocr_runtime_api);
constexpr uint32_t kAdvertisedConfigSize = sizeof(lw_ppocr_config);
constexpr uint32_t kAdvertisedImageSize = sizeof(lw_ppocr_image);
constexpr uint32_t kAdvertisedResultSize = sizeof(lw_ppocr_result);
constexpr uint32_t kAdvertisedCapabilitiesSize = sizeof(lw_ppocr_capabilities);
constexpr uint64_t kAdvertisedFingerprint = LW_PPOCR_ABI_FINGERPRINT;
#endif

const lw_ppocr_runtime_api kRuntimeApi = {
    kAdvertisedRuntimeApiSize,
    LW_PPOCR_RUNTIME_API_VERSION,
    "lw.PPOCR Contract Test Stub",
    &Create,
    &Run,
    &RunJson,
    &ResultFree,
    &StringFree,
    &GetCapabilities,
    &GetLastError,
    &Destroy,
    kAdvertisedConfigSize,
    kAdvertisedImageSize,
    kAdvertisedResultSize,
    kAdvertisedCapabilitiesSize,
    kAdvertisedFingerprint,
    &RecognizeBatch,
    &RecognitionResultFree,
    static_cast<uint32_t>(sizeof(lw_ppocr_recognition)),
    static_cast<uint32_t>(sizeof(lw_ppocr_recognition_result))
};

}  // namespace

lw_ppocr_status LW_PPOCR_CALL lw_ppocr_runtime_get_api(
    uint32_t requested_api_version,
    const lw_ppocr_runtime_api** runtime_api) {
    if (runtime_api == nullptr) {
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *runtime_api = nullptr;
    if (requested_api_version != LW_PPOCR_RUNTIME_API_VERSION) {
        return LW_PPOCR_STATUS_RUNTIME_INCOMPATIBLE;
    }
    *runtime_api = &kRuntimeApi;
    return LW_PPOCR_STATUS_OK;
}
