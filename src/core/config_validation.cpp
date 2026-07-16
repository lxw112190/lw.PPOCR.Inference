#include <lw/ppocr/core/config_validation.hpp>

#include <limits>

namespace lw::ppocr::core {
namespace {

int32_t BytesPerPixel(lw_ppocr_pixel_format format) noexcept {
    switch (format) {
    case LW_PPOCR_PIXEL_FORMAT_GRAY8:
        return 1;
    case LW_PPOCR_PIXEL_FORMAT_BGR24:
    case LW_PPOCR_PIXEL_FORMAT_RGB24:
        return 3;
    case LW_PPOCR_PIXEL_FORMAT_BGRA32:
    case LW_PPOCR_PIXEL_FORMAT_RGBA32:
        return 4;
    default:
        return 0;
    }
}

bool IsKnownBackend(lw_ppocr_backend backend) noexcept {
    return backend >= LW_PPOCR_BACKEND_OPENCV_DNN &&
        backend <= LW_PPOCR_BACKEND_TENSORRT;
}

}  // namespace

lw_ppocr_status ValidateConfig(const lw_ppocr_config* config, std::string& error) {
    if (config == nullptr) {
        error = "config is null";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    if (config->struct_size < LW_PPOCR_CONFIG_V1_SIZE) {
        error = "config struct_size is smaller than the API v1 structure";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    if (config->api_version != LW_PPOCR_API_VERSION) {
        error = "config api_version is not supported";
        return LW_PPOCR_STATUS_RUNTIME_INCOMPATIBLE;
    }
    if (!IsKnownBackend(config->backend)) {
        error = "backend is not supported";
        return LW_PPOCR_STATUS_UNSUPPORTED;
    }
    if (config->model_manifest_utf8 == nullptr || config->model_manifest_utf8[0] == '\0') {
        error = "model_manifest_utf8 is required";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    if (config->limit_side_len <= 0 || config->cls_batch_size <= 0 ||
        config->rec_batch_size <= 0 || config->rec_concurrency <= 0) {
        error = "numeric OCR options must be greater than zero";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    if (config->det_db_thresh < 0.0f || config->det_db_thresh > 1.0f ||
        config->det_db_box_thresh < 0.0f || config->det_db_box_thresh > 1.0f ||
        config->cls_threshold < 0.0f || config->cls_threshold > 1.0f) {
        error = "OCR confidence thresholds must be between zero and one";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    return LW_PPOCR_STATUS_OK;
}

lw_ppocr_status ValidateImage(const lw_ppocr_image* image, std::string& error) {
    if (image == nullptr || image->data == nullptr) {
        error = "image or image data is null";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    if (image->struct_size < LW_PPOCR_IMAGE_V1_SIZE ||
        image->api_version != LW_PPOCR_API_VERSION) {
        error = "image structure is incompatible with API v1";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    if (image->width <= 0 || image->height <= 0 || image->stride <= 0) {
        error = "image dimensions and stride must be greater than zero";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }

    const int32_t bytes_per_pixel = BytesPerPixel(image->pixel_format);
    if (bytes_per_pixel == 0) {
        error = "pixel format is not supported";
        return LW_PPOCR_STATUS_UNSUPPORTED;
    }

    const int64_t minimum_stride =
        static_cast<int64_t>(image->width) * bytes_per_pixel;
    if (image->stride < minimum_stride) {
        error = "image stride is smaller than one packed row";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }

    const uint64_t required_size =
        static_cast<uint64_t>(image->stride) * static_cast<uint64_t>(image->height);
    if (image->data_size < required_size) {
        error = "image data_size is smaller than stride times height";
        return LW_PPOCR_STATUS_INVALID_ARGUMENT;
    }
    return LW_PPOCR_STATUS_OK;
}

const char* BackendDirectory(lw_ppocr_backend backend) noexcept {
    switch (backend) {
    case LW_PPOCR_BACKEND_OPENCV_DNN:
        return "opencv";
    case LW_PPOCR_BACKEND_DIRECTML:
        return "directml";
    case LW_PPOCR_BACKEND_OPENVINO:
        return "openvino";
    case LW_PPOCR_BACKEND_TENSORRT:
        return "tensorrt";
    default:
        return nullptr;
    }
}

const wchar_t* BackendLibraryName(lw_ppocr_backend backend) noexcept {
    switch (backend) {
    case LW_PPOCR_BACKEND_OPENCV_DNN:
        return L"lw.PPOCR.Runtime.OpenCVDNN.dll";
    case LW_PPOCR_BACKEND_DIRECTML:
        return L"lw.PPOCR.Runtime.DirectML.dll";
    case LW_PPOCR_BACKEND_OPENVINO:
        return L"lw.PPOCR.Runtime.OpenVINO.dll";
    case LW_PPOCR_BACKEND_TENSORRT:
        return L"lw.PPOCR.Runtime.TensorRT.dll";
    default:
        return nullptr;
    }
}

}  // namespace lw::ppocr::core
