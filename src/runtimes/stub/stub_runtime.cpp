#include <lw/ppocr/runtime_api.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace {

struct StubEngine {
    lw_ppocr_backend backend = LW_PPOCR_BACKEND_OPENCV_DNN;
    lw_ppocr_log_level log_level = LW_PPOCR_LOG_OFF;
    lw_ppocr_log_callback log_callback = nullptr;
    void* log_user_data = nullptr;
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

void LW_PPOCR_CALL StringFree(void*, char* value) {
    std::free(value);
}

lw_ppocr_status LW_PPOCR_CALL GetCapabilities(
    void* runtime_handle,
    lw_ppocr_capabilities* capabilities) {
    if (runtime_handle == nullptr || capabilities == nullptr ||
        capabilities->struct_size < sizeof(lw_ppocr_capabilities)) {
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

const lw_ppocr_runtime_api kRuntimeApi = {
    sizeof(lw_ppocr_runtime_api),
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
    static_cast<uint32_t>(sizeof(lw_ppocr_config)),
    static_cast<uint32_t>(sizeof(lw_ppocr_image)),
    static_cast<uint32_t>(sizeof(lw_ppocr_result)),
    static_cast<uint32_t>(sizeof(lw_ppocr_capabilities)),
    LW_PPOCR_ABI_FINGERPRINT
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
