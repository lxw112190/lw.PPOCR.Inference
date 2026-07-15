#pragma once

#include <lw/ppocr/core/ocr_types.hpp>
#include <lw/ppocr/ppocr_api.h>

#include <memory>
#include <string>

namespace lw::ppocr::directml {

class DirectMlOcrEngine {
public:
    explicit DirectMlOcrEngine(const lw_ppocr_config& config);
    ~DirectMlOcrEngine();

    DirectMlOcrEngine(const DirectMlOcrEngine&) = delete;
    DirectMlOcrEngine& operator=(const DirectMlOcrEngine&) = delete;

    core::PipelineResult Run(const lw_ppocr_image& image);
    void Log(lw_ppocr_log_level level, const std::string& message) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lw::ppocr::directml
