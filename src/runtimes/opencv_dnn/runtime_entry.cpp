#include "opencv_engine.hpp"

#include <lw/ppocr/runtime_api.h>
#include <lw/ppocr/runtime_result.hpp>

#include <opencv2/core.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <vector>

namespace {

using lw::ppocr::core::PipelineResult;
using lw::ppocr::opencv_dnn::OpenCvOcrEngine;

thread_local std::string g_last_error;

struct OwnedResult {
    lw_ppocr_result value{};
    std::vector<lw_ppocr_text_region> regions;
    std::vector<std::string> texts;
};

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

lw_ppocr_timing ToNativeTiming(const lw::ppocr::core::StageTiming& timing) {
    return {
        timing.preprocess_ms,
        timing.inference_ms,
        timing.postprocess_ms,
        timing.total_ms
    };
}

std::unique_ptr<OwnedResult> ToNativeResult(const PipelineResult& source) {
    auto owned = std::make_unique<OwnedResult>();
    owned->regions.resize(source.regions.size());
    owned->texts.resize(source.regions.size());

    for (size_t index = 0; index < source.regions.size(); ++index) {
        const auto& region = source.regions[index];
        owned->texts[index] = region.text;
        lw_ppocr_text_region& target = owned->regions[index];
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
    owned->value.detector = ToNativeTiming(source.detector);
    owned->value.classifier = ToNativeTiming(source.classifier);
    owned->value.recognizer = ToNativeTiming(source.recognizer);
    owned->value.pipeline = ToNativeTiming(source.pipeline);
    owned->value.reserved_ptr[0] = owned.get();
    return owned;
}

std::string EscapeJson(const std::string& value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20) {
                output << "\\u" << std::hex << std::setw(4)
                       << std::setfill('0') << static_cast<int>(character)
                       << std::dec;
            } else {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    return output.str();
}

std::string ToJson(const PipelineResult& result) {
    std::ostringstream json;
    json << '[';
    for (size_t index = 0; index < result.regions.size(); ++index) {
        if (index > 0) {
            json << ',';
        }
        const auto& region = result.regions[index];
        json << "{\"text\":\"" << EscapeJson(region.text) << "\""
             << ",\"score\":" << region.score;
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
    const lw_ppocr_config* config,
    void** runtime_handle) {
    if (config == nullptr || runtime_handle == nullptr) {
        g_last_error = "OpenCV DNN create received invalid arguments";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *runtime_handle = nullptr;
    if (config->backend != LW_PPOCR_BACKEND_OPENCV_DNN) {
        g_last_error = "OpenCV DNN Runtime received a different backend id";
        return LW_PPOCR_STATUS_UNSUPPORTED;
    }

    return Guard([&]() {
        auto engine = std::make_unique<OpenCvOcrEngine>(*config);
        *runtime_handle = engine.release();
    }, "create OpenCV DNN engine");
}

lw_ppocr_status LW_PPOCR_CALL Run(
    void* runtime_handle,
    const lw_ppocr_image* image,
    lw_ppocr_result** result) {
    if (runtime_handle == nullptr || image == nullptr || result == nullptr) {
        g_last_error = "OpenCV DNN run received invalid arguments";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *result = nullptr;
    return Guard([&]() {
        auto* engine = static_cast<OpenCvOcrEngine*>(runtime_handle);
        auto owned = ToNativeResult(engine->Run(*image));
        *result = &owned->value;
        owned.release();
    }, "run OpenCV DNN inference");
}

lw_ppocr_status LW_PPOCR_CALL RunJson(
    void* runtime_handle,
    const lw_ppocr_image* image,
    char** json_utf8,
    uint64_t* json_length) {
    if (runtime_handle == nullptr || image == nullptr || json_utf8 == nullptr ||
        json_length == nullptr) {
        g_last_error = "OpenCV DNN JSON run received invalid arguments";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *json_utf8 = nullptr;
    *json_length = 0;
    return Guard([&]() {
        auto* engine = static_cast<OpenCvOcrEngine*>(runtime_handle);
        const std::string json = ToJson(engine->Run(*image));
        char* buffer = static_cast<char*>(std::malloc(json.size() + 1));
        if (buffer == nullptr) {
            throw std::bad_alloc();
        }
        std::memcpy(buffer, json.c_str(), json.size() + 1);
        *json_utf8 = buffer;
        *json_length = static_cast<uint64_t>(json.size());
    }, "run OpenCV DNN JSON inference");
}

lw_ppocr_status LW_PPOCR_CALL RecognizeBatch(
    void* runtime_handle, const lw_ppocr_image* images, uint64_t image_count,
    lw_ppocr_recognition_result** result) {
    if (runtime_handle == nullptr || images == nullptr || image_count == 0 ||
        result == nullptr) {
        g_last_error = "OpenCV DNN recognition received invalid arguments";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    *result = nullptr;
    return Guard([&]() {
        auto* engine = static_cast<OpenCvOcrEngine*>(runtime_handle);
        auto owned = lw::ppocr::runtime::ToNativeRecognitionResult(
            engine->RecognizeBatch(images, image_count));
        *result = &owned->value;
        owned.release();
    }, "run OpenCV DNN recognition-only inference");
}

void LW_PPOCR_CALL RecognitionResultFree(
    void*, lw_ppocr_recognition_result* result) {
    lw::ppocr::runtime::FreeRecognitionResult(result);
}

void LW_PPOCR_CALL ResultFree(void*, lw_ppocr_result* result) {
    if (result == nullptr) {
        return;
    }
    delete static_cast<OwnedResult*>(
        const_cast<void*>(result->reserved_ptr[0]));
}

void LW_PPOCR_CALL StringFree(void*, char* value) {
    std::free(value);
}

lw_ppocr_status LW_PPOCR_CALL GetCapabilities(
    void* runtime_handle,
    lw_ppocr_capabilities* capabilities) {
    if (runtime_handle == nullptr || capabilities == nullptr ||
        capabilities->struct_size < LW_PPOCR_CAPABILITIES_V1_SIZE) {
        g_last_error = "OpenCV DNN capabilities received invalid arguments";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    capabilities->api_version = LW_PPOCR_API_VERSION;
    capabilities->backend = LW_PPOCR_BACKEND_OPENCV_DNN;
    capabilities->backend_name_utf8 = "OpenCV DNN";
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
    delete static_cast<OpenCvOcrEngine*>(*runtime_handle);
    *runtime_handle = nullptr;
    g_last_error.clear();
}

const lw_ppocr_runtime_api kRuntimeApi = {
    sizeof(lw_ppocr_runtime_api),
    LW_PPOCR_RUNTIME_API_VERSION,
    "lw.PPOCR OpenCV DNN Runtime",
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
    LW_PPOCR_ABI_FINGERPRINT,
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
