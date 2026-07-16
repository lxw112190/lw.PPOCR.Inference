#pragma once

#include <lw/ppocr/core/ocr_types.hpp>
#include <lw/ppocr/ppocr_api.h>

#include <memory>
#include <string>

namespace lw::ppocr::openvino {

class OpenVinoOcrEngine {
public:
    explicit OpenVinoOcrEngine(const lw_ppocr_config& config);
    ~OpenVinoOcrEngine();

    OpenVinoOcrEngine(const OpenVinoOcrEngine&) = delete;
    OpenVinoOcrEngine& operator=(const OpenVinoOcrEngine&) = delete;

    core::PipelineResult Run(const lw_ppocr_image& image);
    core::RecognitionResult RecognizeBatch(
        const lw_ppocr_image* images, uint64_t image_count);
    void Log(lw_ppocr_log_level level, const std::string& message) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lw::ppocr::openvino
