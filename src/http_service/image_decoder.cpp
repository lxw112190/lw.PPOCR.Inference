#include "image_decoder.h"

#include <cstring>
#include <limits>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#else
#include <opencv2/imgcodecs.hpp>
#endif

namespace lw::ppocr::http {
namespace {

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;

std::string HResultMessage(const char* operation, HRESULT result) {
    std::ostringstream stream;
    stream << operation << " failed (HRESULT=0x" << std::hex
           << static_cast<uint32_t>(result) << ')';
    return stream.str();
}

class ComApartment {
public:
    ComApartment() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~ComApartment() {
        if (result_ == S_OK || result_ == S_FALSE) {
            CoUninitialize();
        }
    }
    HRESULT result() const noexcept { return result_; }

private:
    HRESULT result_;
};
#endif

}  // namespace

bool DecodeImage(const std::vector<uint8_t>& encoded,
                 uint64_t maximum_pixels,
                 DecodedImage& image,
                 std::string& error) {
    image = {};
    error.clear();

    if (encoded.empty()) {
        error = "encoded image is empty";
        return false;
    }

#if defined(_WIN32)
    ComApartment apartment;
    if (FAILED(apartment.result()) && apartment.result() != RPC_E_CHANGED_MODE) {
        error = HResultMessage("CoInitializeEx", apartment.result());
        return false;
    }

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, encoded.size());
    if (memory == nullptr) {
        error = "unable to allocate image stream";
        return false;
    }
    void* destination = GlobalLock(memory);
    if (destination == nullptr) {
        GlobalFree(memory);
        error = "unable to lock image stream";
        return false;
    }
    std::memcpy(destination, encoded.data(), encoded.size());
    GlobalUnlock(memory);

    ComPtr<IStream> stream;
    HRESULT result = CreateStreamOnHGlobal(memory, TRUE, &stream);
    if (FAILED(result)) {
        GlobalFree(memory);
        error = HResultMessage("CreateStreamOnHGlobal", result);
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    result = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(result)) {
        error = HResultMessage("WIC factory creation", result);
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    result = factory->CreateDecoderFromStream(stream.Get(), nullptr,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(result)) {
        error = "unsupported or corrupt image (JPEG, PNG, BMP, GIF and TIFF are supported)";
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, &frame);
    if (FAILED(result)) {
        error = HResultMessage("WIC frame decoding", result);
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    result = frame->GetSize(&width, &height);
    if (FAILED(result) || width == 0 || height == 0) {
        error = "image dimensions are invalid";
        return false;
    }
    const uint64_t pixel_count =
        static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    if (pixel_count > maximum_pixels) {
        error = "image dimensions exceed the configured pixel limit";
        return false;
    }
    if (width > (std::numeric_limits<UINT>::max)() / 3u) {
        error = "image width is too large";
        return false;
    }

    ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(&converter);
    if (FAILED(result)) {
        error = HResultMessage("WIC converter creation", result);
        return false;
    }
    result = converter->Initialize(frame.Get(), GUID_WICPixelFormat24bppBGR,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(result)) {
        error = HResultMessage("WIC BGR conversion", result);
        return false;
    }

    const UINT stride = width * 3u;
    const uint64_t buffer_size =
        static_cast<uint64_t>(stride) * static_cast<uint64_t>(height);
    if (buffer_size > (std::numeric_limits<UINT>::max)()) {
        error = "decoded image is too large";
        return false;
    }
    image.pixels.resize(static_cast<size_t>(buffer_size));
    result = converter->CopyPixels(nullptr, stride,
        static_cast<UINT>(buffer_size), image.pixels.data());
    if (FAILED(result)) {
        image = {};
        error = HResultMessage("WIC pixel copy", result);
        return false;
    }

    image.width = width;
    image.height = height;
    image.stride = stride;
    return true;
#else
    if (encoded.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        error = "encoded image is too large";
        return false;
    }
    const cv::Mat encoded_view(1, static_cast<int>(encoded.size()), CV_8UC1,
        const_cast<uint8_t*>(encoded.data()));
    const cv::Mat decoded = cv::imdecode(encoded_view, cv::IMREAD_COLOR);
    if (decoded.empty()) {
        error = "unsupported or corrupt image (JPEG, PNG, BMP, WebP and TIFF depend on the OpenCV build)";
        return false;
    }
    if (decoded.cols <= 0 || decoded.rows <= 0 || decoded.channels() != 3) {
        error = "decoded image dimensions or pixel format are invalid";
        return false;
    }
    const uint64_t width = static_cast<uint64_t>(decoded.cols);
    const uint64_t height = static_cast<uint64_t>(decoded.rows);
    if (width * height > maximum_pixels) {
        error = "image dimensions exceed the configured pixel limit";
        return false;
    }
    if (width > (std::numeric_limits<uint32_t>::max)() / 3u) {
        error = "image width is too large";
        return false;
    }
    const uint64_t stride = width * 3u;
    if (height > (std::numeric_limits<size_t>::max)() / stride) {
        error = "decoded image is too large";
        return false;
    }
    image.pixels.resize(static_cast<size_t>(stride * height));
    for (int row = 0; row < decoded.rows; ++row) {
        std::memcpy(image.pixels.data() + static_cast<size_t>(row) * stride,
            decoded.ptr(row), static_cast<size_t>(stride));
    }
    image.width = static_cast<uint32_t>(width);
    image.height = static_cast<uint32_t>(height);
    image.stride = static_cast<uint32_t>(stride);
    return true;
#endif
}

}  // namespace lw::ppocr::http
