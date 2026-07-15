#include <lw/ppocr/ppocr_api.h>
#include <lw/ppocr/core/config_validation.hpp>
#include <lw/ppocr/runtime_api.h>

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

struct lw_ppocr_engine {
    HMODULE module = nullptr;
    const lw_ppocr_runtime_api* runtime_api = nullptr;
    void* runtime_handle = nullptr;
};

namespace {

thread_local std::string g_last_error;

void SetLastError(std::string message) {
    g_last_error = std::move(message);
}

void ClearLastError() {
    g_last_error.clear();
}

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

std::wstring Utf8ToWide(const char* value) {
    if (value == nullptr || value[0] == '\0') {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1,
        nullptr, 0);
    if (length <= 0) {
        throw std::runtime_error("a path is not valid UTF-8");
    }

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1,
        result.data(), length);
    result.resize(result.size() - 1);
    return result;
}

std::filesystem::path LoaderDirectory() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetModuleFileNameW(
            reinterpret_cast<HMODULE>(&__ImageBase), buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw std::runtime_error("GetModuleFileNameW failed");
        }
        if (length < buffer.size() - 1) {
            return std::filesystem::path(buffer.data()).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path RuntimeLibraryPath(const lw_ppocr_config& config) {
    if (config.runtime_library_utf8 != nullptr &&
        config.runtime_library_utf8[0] != '\0') {
        std::filesystem::path explicit_path(Utf8ToWide(config.runtime_library_utf8));
        if (explicit_path.is_relative()) {
            explicit_path = std::filesystem::absolute(explicit_path);
        }
        return explicit_path.lexically_normal();
    }

    const wchar_t* library_name = lw::ppocr::core::BackendLibraryName(config.backend);
    const char* backend_directory = lw::ppocr::core::BackendDirectory(config.backend);
    if (library_name == nullptr || backend_directory == nullptr) {
        throw std::runtime_error("no Runtime library is registered for this backend");
    }

    std::filesystem::path root;
    if (config.runtime_root_utf8 != nullptr && config.runtime_root_utf8[0] != '\0') {
        root = std::filesystem::path(Utf8ToWide(config.runtime_root_utf8));
    } else {
        root = LoaderDirectory() / L"runtimes" / L"win-x64";
    }

    return (root / Utf8ToWide(backend_directory) / library_name).lexically_normal();
}

std::string WindowsErrorMessage(DWORD error_code) {
    wchar_t* wide_message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error_code, 0, reinterpret_cast<wchar_t*>(&wide_message), 0, nullptr);

    if (length == 0 || wide_message == nullptr) {
        return "Windows error " + std::to_string(error_code);
    }

    const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide_message,
        static_cast<int>(length), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(utf8_length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide_message, static_cast<int>(length),
        result.data(), utf8_length, nullptr, nullptr);
    LocalFree(wide_message);

    while (!result.empty() &&
        (result.back() == '\r' || result.back() == '\n' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

std::string RuntimeLastError(lw_ppocr_handle handle) {
    if (handle == nullptr || handle->runtime_api == nullptr ||
        handle->runtime_api->get_last_error == nullptr) {
        return {};
    }

    const uint64_t required = handle->runtime_api->get_last_error(
        handle->runtime_handle, nullptr, 0);
    if (required <= 1 || required > static_cast<uint64_t>(SIZE_MAX)) {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(required));
    handle->runtime_api->get_last_error(
        handle->runtime_handle, buffer.data(), required);
    return std::string(buffer.data());
}

bool RuntimeApiIsComplete(const lw_ppocr_runtime_api* api) noexcept {
    return api != nullptr &&
        api->struct_size >= sizeof(lw_ppocr_runtime_api) &&
        api->api_version == LW_PPOCR_RUNTIME_API_VERSION &&
        api->create != nullptr && api->run != nullptr &&
        api->run_json != nullptr && api->result_free != nullptr &&
        api->string_free != nullptr && api->get_capabilities != nullptr &&
        api->get_last_error != nullptr && api->destroy != nullptr &&
        api->public_config_size == sizeof(lw_ppocr_config) &&
        api->public_image_size == sizeof(lw_ppocr_image) &&
        api->public_result_size == sizeof(lw_ppocr_result) &&
        api->public_capabilities_size == sizeof(lw_ppocr_capabilities) &&
        api->public_abi_fingerprint == LW_PPOCR_ABI_FINGERPRINT;
}

}  // namespace

lw_ppocr_status LW_PPOCR_CALL lw_ppocr_get_version(lw_ppocr_version* version) {
    if (version == nullptr || version->struct_size < sizeof(lw_ppocr_version)) {
        SetLastError("version structure is null or too small");
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }

    version->api_version = LW_PPOCR_API_VERSION;
    version->major = LW_PPOCR_VERSION_MAJOR;
    version->minor = LW_PPOCR_VERSION_MINOR;
    version->patch = LW_PPOCR_VERSION_PATCH;
    version->product_name_utf8 = "lw.PPOCR.Inference";
    version->version_utf8 = "0.1.0";
    ClearLastError();
    return LW_PPOCR_STATUS_OK;
}

lw_ppocr_status LW_PPOCR_CALL lw_ppocr_create(
    const lw_ppocr_config* config,
    lw_ppocr_handle* handle) {
    if (handle == nullptr) {
        SetLastError("handle output is null");
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *handle = nullptr;

    std::string validation_error;
    const lw_ppocr_status validation =
        lw::ppocr::core::ValidateConfig(config, validation_error);
    if (validation != LW_PPOCR_STATUS_OK) {
        SetLastError(validation_error);
        return validation;
    }

    try {
        const std::filesystem::path runtime_path = RuntimeLibraryPath(*config);
        HMODULE module = LoadLibraryExW(runtime_path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (module == nullptr) {
            SetLastError("failed to load Runtime '" + runtime_path.u8string() +
                "': " + WindowsErrorMessage(GetLastError()));
            return LW_PPOCR_STATUS_RUNTIME_NOT_FOUND;
        }

        const auto get_api = reinterpret_cast<lw_ppocr_runtime_get_api_fn>(
            GetProcAddress(module, LW_PPOCR_RUNTIME_ENTRY_NAME));
        if (get_api == nullptr) {
            FreeLibrary(module);
            SetLastError("Runtime does not export " LW_PPOCR_RUNTIME_ENTRY_NAME);
            return LW_PPOCR_STATUS_RUNTIME_INCOMPATIBLE;
        }

        const lw_ppocr_runtime_api* runtime_api = nullptr;
        const lw_ppocr_status api_status = get_api(
            LW_PPOCR_RUNTIME_API_VERSION, &runtime_api);
        if (api_status != LW_PPOCR_STATUS_OK || !RuntimeApiIsComplete(runtime_api)) {
            FreeLibrary(module);
            SetLastError("Runtime API is incomplete or incompatible");
            return LW_PPOCR_STATUS_RUNTIME_INCOMPATIBLE;
        }

        auto* engine = new (std::nothrow) lw_ppocr_engine();
        if (engine == nullptr) {
            FreeLibrary(module);
            SetLastError("failed to allocate loader engine");
            return LW_PPOCR_STATUS_OUT_OF_MEMORY;
        }

        engine->module = module;
        engine->runtime_api = runtime_api;
        const lw_ppocr_status create_status =
            runtime_api->create(config, &engine->runtime_handle);
        if (create_status != LW_PPOCR_STATUS_OK || engine->runtime_handle == nullptr) {
            const std::string runtime_error = RuntimeLastError(engine);
            if (engine->runtime_handle != nullptr) {
                runtime_api->destroy(&engine->runtime_handle);
            }
            FreeLibrary(module);
            delete engine;
            SetLastError(runtime_error.empty() ? "Runtime failed to create an engine" :
                runtime_error);
            return create_status == LW_PPOCR_STATUS_OK
                ? LW_PPOCR_STATUS_INTERNAL_ERROR
                : create_status;
        }

        *handle = engine;
        ClearLastError();
        return LW_PPOCR_STATUS_OK;
    } catch (const std::bad_alloc&) {
        SetLastError("out of memory while loading Runtime");
        return LW_PPOCR_STATUS_OUT_OF_MEMORY;
    } catch (const std::exception& exception) {
        SetLastError(exception.what());
        return LW_PPOCR_STATUS_INTERNAL_ERROR;
    } catch (...) {
        SetLastError("unknown error while loading Runtime");
        return LW_PPOCR_STATUS_INTERNAL_ERROR;
    }
}

lw_ppocr_status LW_PPOCR_CALL lw_ppocr_run(
    lw_ppocr_handle handle,
    const lw_ppocr_image* image,
    lw_ppocr_result** result) {
    if (result == nullptr) {
        SetLastError("result output is null");
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *result = nullptr;
    if (handle == nullptr) {
        SetLastError("engine handle is null");
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }

    std::string validation_error;
    const lw_ppocr_status validation =
        lw::ppocr::core::ValidateImage(image, validation_error);
    if (validation != LW_PPOCR_STATUS_OK) {
        SetLastError(validation_error);
        return validation;
    }

    const lw_ppocr_status status =
        handle->runtime_api->run(handle->runtime_handle, image, result);
    if (status == LW_PPOCR_STATUS_OK) {
        ClearLastError();
    } else {
        const std::string runtime_error = RuntimeLastError(handle);
        SetLastError(runtime_error.empty() ? "Runtime inference failed" : runtime_error);
    }
    return status;
}

lw_ppocr_status LW_PPOCR_CALL lw_ppocr_run_json(
    lw_ppocr_handle handle,
    const lw_ppocr_image* image,
    char** json_utf8,
    uint64_t* json_length) {
    if (json_utf8 == nullptr || json_length == nullptr) {
        SetLastError("JSON outputs are null");
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *json_utf8 = nullptr;
    *json_length = 0;
    if (handle == nullptr) {
        SetLastError("engine handle is null");
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }

    std::string validation_error;
    const lw_ppocr_status validation =
        lw::ppocr::core::ValidateImage(image, validation_error);
    if (validation != LW_PPOCR_STATUS_OK) {
        SetLastError(validation_error);
        return validation;
    }

    const lw_ppocr_status status = handle->runtime_api->run_json(
        handle->runtime_handle, image, json_utf8, json_length);
    if (status == LW_PPOCR_STATUS_OK) {
        ClearLastError();
    } else {
        const std::string runtime_error = RuntimeLastError(handle);
        SetLastError(runtime_error.empty() ? "Runtime JSON inference failed" : runtime_error);
    }
    return status;
}

void LW_PPOCR_CALL lw_ppocr_result_free(
    lw_ppocr_handle handle,
    lw_ppocr_result* result) {
    if (handle != nullptr && result != nullptr) {
        handle->runtime_api->result_free(handle->runtime_handle, result);
    }
}

void LW_PPOCR_CALL lw_ppocr_string_free(lw_ppocr_handle handle, char* value) {
    if (handle != nullptr && value != nullptr) {
        handle->runtime_api->string_free(handle->runtime_handle, value);
    }
}

lw_ppocr_status LW_PPOCR_CALL lw_ppocr_get_capabilities(
    lw_ppocr_handle handle,
    lw_ppocr_capabilities* capabilities) {
    if (handle == nullptr || capabilities == nullptr ||
        capabilities->struct_size < sizeof(lw_ppocr_capabilities)) {
        SetLastError("engine or capabilities structure is invalid");
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }

    const lw_ppocr_status status = handle->runtime_api->get_capabilities(
        handle->runtime_handle, capabilities);
    if (status == LW_PPOCR_STATUS_OK) {
        ClearLastError();
    } else {
        const std::string runtime_error = RuntimeLastError(handle);
        SetLastError(runtime_error.empty() ? "Runtime capability query failed" : runtime_error);
    }
    return status;
}

uint64_t LW_PPOCR_CALL lw_ppocr_get_last_error(
    lw_ppocr_handle,
    char* buffer_utf8,
    uint64_t buffer_capacity) {
    return CopyText(g_last_error, buffer_utf8, buffer_capacity);
}

void LW_PPOCR_CALL lw_ppocr_destroy(lw_ppocr_handle* handle) {
    if (handle == nullptr || *handle == nullptr) {
        return;
    }

    lw_ppocr_engine* engine = *handle;
    *handle = nullptr;
    if (engine->runtime_api != nullptr && engine->runtime_api->destroy != nullptr) {
        engine->runtime_api->destroy(&engine->runtime_handle);
    }
    if (engine->module != nullptr) {
        FreeLibrary(engine->module);
    }
    delete engine;
    ClearLastError();
}
