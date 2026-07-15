#pragma once

#include <lw/ppocr/core/ocr_types.hpp>
#include <lw/ppocr/ppocr_api.h>

#include <memory>
#include <string>

namespace lw::ppocr::opencv_dnn {

class OpenCvOcrEngine {
public:
    explicit OpenCvOcrEngine(const lw_ppocr_config& config);
    ~OpenCvOcrEngine();

    OpenCvOcrEngine(const OpenCvOcrEngine&) = delete;
    OpenCvOcrEngine& operator=(const OpenCvOcrEngine&) = delete;

    core::PipelineResult Run(const lw_ppocr_image& image);
    void Log(lw_ppocr_log_level level, const std::string& message) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lw::ppocr::opencv_dnn
