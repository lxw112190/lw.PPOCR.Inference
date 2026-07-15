#pragma once

#include <lw/ppocr/ppocr_api.h>

#include <string>

namespace lw::ppocr::core {

lw_ppocr_status ValidateConfig(const lw_ppocr_config* config, std::string& error);
lw_ppocr_status ValidateImage(const lw_ppocr_image* image, std::string& error);
const char* BackendDirectory(lw_ppocr_backend backend) noexcept;
const wchar_t* BackendLibraryName(lw_ppocr_backend backend) noexcept;

}  // namespace lw::ppocr::core

