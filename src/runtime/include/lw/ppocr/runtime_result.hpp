#pragma once

#include <lw/ppocr/core/ocr_types.hpp>
#include <lw/ppocr/ppocr_api.h>

#include <memory>
#include <string>
#include <vector>

namespace lw::ppocr::runtime {

struct OwnedRecognitionResult {
    lw_ppocr_recognition_result value{};
    std::vector<lw_ppocr_recognition> items;
    std::vector<std::string> texts;
};

inline lw_ppocr_timing ToNativeTiming(const core::StageTiming& timing) {
    return {timing.preprocess_ms, timing.inference_ms,
        timing.postprocess_ms, timing.total_ms};
}

inline std::unique_ptr<OwnedRecognitionResult> ToNativeRecognitionResult(
    const core::RecognitionResult& source) {
    auto owned = std::make_unique<OwnedRecognitionResult>();
    owned->items.resize(source.items.size());
    owned->texts.resize(source.items.size());
    for (size_t index = 0; index < source.items.size(); ++index) {
        const core::OcrRegion& source_item = source.items[index];
        owned->texts[index] = source_item.text;
        lw_ppocr_recognition& target = owned->items[index];
        target.struct_size = sizeof(lw_ppocr_recognition);
        target.api_version = LW_PPOCR_API_VERSION;
        target.source_index = static_cast<uint64_t>(index);
        target.text_utf8 = owned->texts[index].c_str();
        target.score = source_item.score;
        target.cls_label = source_item.cls_label;
        target.cls_score = source_item.cls_score;
    }
    owned->value.struct_size = sizeof(lw_ppocr_recognition_result);
    owned->value.api_version = LW_PPOCR_API_VERSION;
    owned->value.item_count = static_cast<uint64_t>(owned->items.size());
    owned->value.items = owned->items.data();
    owned->value.classifier = ToNativeTiming(source.classifier);
    owned->value.recognizer = ToNativeTiming(source.recognizer);
    owned->value.pipeline = ToNativeTiming(source.pipeline);
    owned->value.reserved_ptr[0] = owned.get();
    return owned;
}

inline void FreeRecognitionResult(lw_ppocr_recognition_result* result) {
    if (result != nullptr) {
        delete static_cast<OwnedRecognitionResult*>(
            const_cast<void*>(result->reserved_ptr[0]));
    }
}

}  // namespace lw::ppocr::runtime
