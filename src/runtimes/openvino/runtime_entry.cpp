#include "openvino_engine.hpp"

#include <lw/ppocr/runtime_api.h>

#include <openvino/openvino.hpp>
#include <opencv2/core.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

using lw::ppocr::core::PipelineResult;
using lw::ppocr::openvino::OpenVinoOcrEngine;

thread_local std::string g_last_error;

struct OwnedResult {
    lw_ppocr_result value{};
    std::vector<lw_ppocr_text_region> regions;
    std::vector<std::string> texts;
};

uint64_t CopyText(const std::string& text, char* buffer, uint64_t capacity) {
    const uint64_t required = static_cast<uint64_t>(text.size()) + 1;
    if (buffer && capacity > 0) {
        const size_t count = static_cast<size_t>(
            std::min<uint64_t>(capacity - 1, text.size()));
        if (count > 0) std::memcpy(buffer, text.data(), count);
        buffer[count] = '\0';
    }
    return required;
}

lw_ppocr_timing NativeTiming(const lw::ppocr::core::StageTiming& value) {
    return {value.preprocess_ms, value.inference_ms,
        value.postprocess_ms, value.total_ms};
}

std::unique_ptr<OwnedResult> NativeResult(const PipelineResult& source) {
    auto owned = std::make_unique<OwnedResult>();
    owned->regions.resize(source.regions.size());
    owned->texts.resize(source.regions.size());
    for (size_t index = 0; index < source.regions.size(); ++index) {
        const auto& region = source.regions[index];
        owned->texts[index] = region.text;
        auto& target = owned->regions[index];
        target.box.top_left = {region.box[0].x, region.box[0].y};
        target.box.top_right = {region.box[1].x, region.box[1].y};
        target.box.bottom_right = {region.box[2].x, region.box[2].y};
        target.box.bottom_left = {region.box[3].x, region.box[3].y};
        target.text_utf8 = owned->texts[index].c_str();
        target.score = region.score;
        target.cls_label = region.cls_label;
        target.cls_score = region.cls_score;
    }
    owned->value.struct_size = sizeof(lw_ppocr_result);
    owned->value.api_version = LW_PPOCR_API_VERSION;
    owned->value.region_count = static_cast<uint64_t>(owned->regions.size());
    owned->value.regions = owned->regions.data();
    owned->value.detector = NativeTiming(source.detector);
    owned->value.classifier = NativeTiming(source.classifier);
    owned->value.recognizer = NativeTiming(source.recognizer);
    owned->value.pipeline = NativeTiming(source.pipeline);
    owned->value.reserved_ptr[0] = owned.get();
    return owned;
}

std::string EscapeJson(const std::string& text) {
    std::ostringstream result;
    for (const unsigned char character : text) {
        if (character == '"') result << "\\\"";
        else if (character == '\\') result << "\\\\";
        else if (character == '\n') result << "\\n";
        else if (character == '\r') result << "\\r";
        else if (character == '\t') result << "\\t";
        else if (character < 0x20) {
            result << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                   << static_cast<int>(character) << std::dec;
        } else result << static_cast<char>(character);
    }
    return result.str();
}

std::string JsonResult(const PipelineResult& source) {
    std::ostringstream json;
    json << '[';
    for (size_t index = 0; index < source.regions.size(); ++index) {
        if (index > 0) json << ',';
        const auto& region = source.regions[index];
        json << "{\"text\":\"" << EscapeJson(region.text)
             << "\",\"score\":" << region.score;
        for (size_t point = 0; point < region.box.size(); ++point) {
            json << ",\"x" << point + 1 << "\":" << region.box[point].x
                 << ",\"y" << point + 1 << "\":" << region.box[point].y;
        }
        json << ",\"cls_label\":" << region.cls_label
             << ",\"cls_score\":" << region.cls_score << '}';
    }
    json << ']';
    return json.str();
}

template <typename Function>
lw_ppocr_status Guard(Function&& function, const char* operation) {
    try {
        function();
        g_last_error.clear();
        return LW_PPOCR_STATUS_OK;
    } catch (const ov::Exception& exception) {
        g_last_error = std::string(operation) + ": OpenVINO: " + exception.what();
        return LW_PPOCR_STATUS_INFERENCE_ERROR;
    } catch (const cv::Exception& exception) {
        g_last_error = std::string(operation) + ": OpenCV: " + exception.what();
        return LW_PPOCR_STATUS_INFERENCE_ERROR;
    } catch (const std::bad_alloc&) {
        g_last_error = std::string(operation) + ": out of memory";
        return LW_PPOCR_STATUS_OUT_OF_MEMORY;
    } catch (const std::exception& exception) {
        g_last_error = std::string(operation) + ": " + exception.what();
        return LW_PPOCR_STATUS_INFERENCE_ERROR;
    } catch (...) {
        g_last_error = std::string(operation) + ": unknown error";
        return LW_PPOCR_STATUS_INTERNAL_ERROR;
    }
}

lw_ppocr_status LW_PPOCR_CALL Create(
    const lw_ppocr_config* config, void** runtime_handle) {
    if (!config || !runtime_handle) {
        g_last_error = "OpenVINO create received invalid arguments";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *runtime_handle = nullptr;
    if (config->backend != LW_PPOCR_BACKEND_OPENVINO) {
        g_last_error = "OpenVINO Runtime received a different backend id";
        return LW_PPOCR_STATUS_UNSUPPORTED;
    }
    return Guard([&] {
        auto engine = std::make_unique<OpenVinoOcrEngine>(*config);
        *runtime_handle = engine.release();
    }, "create OpenVINO engine");
}

lw_ppocr_status LW_PPOCR_CALL Run(
    void* handle, const lw_ppocr_image* image, lw_ppocr_result** result) {
    if (!handle || !image || !result) {
        g_last_error = "OpenVINO run received invalid arguments";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *result = nullptr;
    return Guard([&] {
        auto owned = NativeResult(
            static_cast<OpenVinoOcrEngine*>(handle)->Run(*image));
        *result = &owned->value;
        owned.release();
    }, "run OpenVINO inference");
}

lw_ppocr_status LW_PPOCR_CALL RunJson(void* handle,
    const lw_ppocr_image* image, char** json_utf8, uint64_t* json_length) {
    if (!handle || !image || !json_utf8 || !json_length) {
        g_last_error = "OpenVINO JSON run received invalid arguments";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *json_utf8 = nullptr;
    *json_length = 0;
    return Guard([&] {
        const std::string json = JsonResult(
            static_cast<OpenVinoOcrEngine*>(handle)->Run(*image));
        auto* output = static_cast<char*>(std::malloc(json.size() + 1));
        if (!output) throw std::bad_alloc();
        std::memcpy(output, json.c_str(), json.size() + 1);
        *json_utf8 = output;
        *json_length = static_cast<uint64_t>(json.size());
    }, "run OpenVINO JSON inference");
}

void LW_PPOCR_CALL ResultFree(void*, lw_ppocr_result* result) {
    if (result) {
        delete static_cast<OwnedResult*>(
            const_cast<void*>(result->reserved_ptr[0]));
    }
}

void LW_PPOCR_CALL StringFree(void*, char* value) { std::free(value); }

lw_ppocr_status LW_PPOCR_CALL GetCapabilities(
    void* handle, lw_ppocr_capabilities* capabilities) {
    if (!handle || !capabilities ||
        capabilities->struct_size < sizeof(lw_ppocr_capabilities)) {
        g_last_error = "OpenVINO capabilities received invalid arguments";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    capabilities->api_version = LW_PPOCR_API_VERSION;
    capabilities->backend = LW_PPOCR_BACKEND_OPENVINO;
    capabilities->backend_name_utf8 = "OpenVINO";
    capabilities->supports_cpu = 1;
    capabilities->supports_gpu = 0;
    capabilities->supports_fp16 = 1;
    capabilities->supports_int8 = 1;
    capabilities->supports_cls = 1;
    g_last_error.clear();
    return LW_PPOCR_STATUS_OK;
}

uint64_t LW_PPOCR_CALL GetLastError(void*, char* buffer, uint64_t capacity) {
    return CopyText(g_last_error, buffer, capacity);
}

void LW_PPOCR_CALL Destroy(void** handle) {
    if (!handle || !*handle) return;
    delete static_cast<OpenVinoOcrEngine*>(*handle);
    *handle = nullptr;
    g_last_error.clear();
}

const lw_ppocr_runtime_api kRuntimeApi = {
    sizeof(lw_ppocr_runtime_api), LW_PPOCR_RUNTIME_API_VERSION,
    "lw.PPOCR OpenVINO Runtime",
    &Create, &Run, &RunJson, &ResultFree, &StringFree,
    &GetCapabilities, &GetLastError, &Destroy,
    static_cast<uint32_t>(sizeof(lw_ppocr_config)),
    static_cast<uint32_t>(sizeof(lw_ppocr_image)),
    static_cast<uint32_t>(sizeof(lw_ppocr_result)),
    static_cast<uint32_t>(sizeof(lw_ppocr_capabilities)),
    LW_PPOCR_ABI_FINGERPRINT
};

}  // namespace

lw_ppocr_status LW_PPOCR_CALL lw_ppocr_runtime_get_api(
    uint32_t version, const lw_ppocr_runtime_api** runtime_api) {
    if (!runtime_api) return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    *runtime_api = nullptr;
    if (version != LW_PPOCR_RUNTIME_API_VERSION) {
        return LW_PPOCR_STATUS_RUNTIME_INCOMPATIBLE;
    }
    *runtime_api = &kRuntimeApi;
    return LW_PPOCR_STATUS_OK;
}
