#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lw::ppocr::http {

struct DecodedImage {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
};

bool DecodeImage(const std::vector<uint8_t>& encoded,
                 uint64_t maximum_pixels,
                 DecodedImage& image,
                 std::string& error);

}  // namespace lw::ppocr::http
