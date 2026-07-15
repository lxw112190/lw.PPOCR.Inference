#include "tensorrt_session.hpp"

#include <lw/ppocr/core/model_manifest.hpp>

#include <NvInferPlugin.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace lw::ppocr::tensorrt {
namespace {

void CheckCuda(cudaError_t status, const char* operation) {
    if (status != cudaSuccess) {
        throw std::runtime_error(
            std::string(operation) + ": " + cudaGetErrorString(status));
    }
}

}  // namespace

void TensorRtLogger::log(Severity severity, const char* message) noexcept {
    if (severity <= Severity::kWARNING && message != nullptr) {
        std::cerr << "[TensorRT] " << message << '\n';
    }
}

TensorRtEnvironment::TensorRtEnvironment(const lw_ppocr_config& config)
    : device_id_(config.device_id) {
    CheckCuda(cudaSetDevice(device_id_), "cudaSetDevice");
    if (!initLibNvInferPlugins(&logger_, "")) {
        throw std::runtime_error("failed to initialize TensorRT plugins");
    }
    runtime_.reset(nvinfer1::createInferRuntime(logger_));
    if (!runtime_) {
        throw std::runtime_error("failed to create TensorRT runtime");
    }
}

std::shared_ptr<nvinfer1::ICudaEngine> TensorRtEnvironment::LoadEngine(
    const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(load_mutex_);
    const std::filesystem::path key = std::filesystem::absolute(path).lexically_normal();
    const auto existing = engines_.find(key);
    if (existing != engines_.end()) {
        return existing->second;
    }
    CheckCuda(cudaSetDevice(device_id_), "cudaSetDevice");
    const std::vector<unsigned char> bytes = core::ReadBinaryFile(key);
    auto engine = std::shared_ptr<nvinfer1::ICudaEngine>(
        runtime_->deserializeCudaEngine(bytes.data(), bytes.size()));
    if (!engine) {
        throw std::runtime_error(
            "failed to deserialize TensorRT engine: " + core::PathToUtf8(key));
    }
    engines_.emplace(key, engine);
    return engine;
}

int TensorRtEnvironment::RecognitionConcurrency(int requested) const noexcept {
    return std::max(1, std::min(requested, 4));
}

TensorRtSession::TensorRtSession(
    TensorRtEnvironment& environment,
    const std::filesystem::path& engine_path,
    int device_id,
    bool)
    : device_id_(device_id), engine_(environment.LoadEngine(engine_path)) {
    SetDevice();
    context_.reset(engine_->createExecutionContext());
    if (!context_) {
        throw std::runtime_error("failed to create TensorRT execution context");
    }
    for (int32_t index = 0; index < engine_->getNbIOTensors(); ++index) {
        const char* name = engine_->getIOTensorName(index);
        if (name == nullptr || engine_->getTensorDataType(name) !=
                nvinfer1::DataType::kFLOAT) {
            throw std::runtime_error(
                "TensorRT Runtime requires FP32 input and output tensors");
        }
        if (engine_->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT &&
            input_name_.empty()) {
            input_name_ = name;
        } else if (engine_->getTensorIOMode(name) ==
                nvinfer1::TensorIOMode::kOUTPUT && output_name_.empty()) {
            output_name_ = name;
        }
    }
    if (input_name_.empty() || output_name_.empty()) {
        throw std::runtime_error(
            "TensorRT engine must expose an input and an output tensor");
    }
    nvinfer1::Dims maximum = engine_->getProfileShape(
        input_name_.c_str(), 0, nvinfer1::OptProfileSelector::kMAX);
    if (maximum.nbDims < 0) {
        maximum = engine_->getTensorShape(input_name_.c_str());
    }
    maximum_input_shape_ = ToShape(maximum);
    CheckCuda(cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking),
        "cudaStreamCreateWithFlags");
}

TensorRtSession::~TensorRtSession() {
    try {
        SetDevice();
    } catch (...) {
    }
    ReleaseBuffers();
    if (stream_ != nullptr) {
        cudaStreamDestroy(stream_);
        stream_ = nullptr;
    }
}

TensorRtOutput TensorRtSession::Run(
    float* input,
    size_t element_count,
    const std::vector<int64_t>& shape) {
    SetDevice();
    if (!context_->setInputShape(input_name_.c_str(), ToDims(shape))) {
        throw std::runtime_error(
            "TensorRT input shape is outside the engine optimization profile");
    }
    const std::vector<int64_t> output_shape =
        ToShape(context_->getTensorShape(output_name_.c_str()));
    const size_t expected_input = Count(shape);
    const size_t output_count = Count(output_shape);
    if (expected_input == 0 || expected_input != element_count || output_count == 0) {
        throw std::runtime_error("TensorRT tensor shape is invalid");
    }

    const size_t input_bytes = element_count * sizeof(float);
    const size_t output_bytes = output_count * sizeof(float);
    EnsureHostBuffer(&input_host_, input_host_capacity_, input_bytes);
    EnsureHostBuffer(&output_host_, output_host_capacity_, output_bytes);
    EnsureDeviceBuffer(&input_device_, input_device_capacity_, input_bytes);
    EnsureDeviceBuffer(&output_device_, output_device_capacity_, output_bytes);
    std::memcpy(input_host_, input, input_bytes);

    CheckCuda(cudaMemcpyAsync(input_device_, input_host_, input_bytes,
        cudaMemcpyHostToDevice, stream_), "cudaMemcpyAsync H2D");
    if (!context_->setTensorAddress(input_name_.c_str(), input_device_) ||
        !context_->setTensorAddress(output_name_.c_str(), output_device_)) {
        throw std::runtime_error("TensorRT failed to bind tensor addresses");
    }
    if (!context_->enqueueV3(stream_)) {
        throw std::runtime_error("TensorRT enqueueV3 failed");
    }
    CheckCuda(cudaMemcpyAsync(output_host_, output_device_, output_bytes,
        cudaMemcpyDeviceToHost, stream_), "cudaMemcpyAsync D2H");
    CheckCuda(cudaStreamSynchronize(stream_), "cudaStreamSynchronize");

    return {static_cast<const float*>(output_host_), output_shape, output_count};
}

int64_t TensorRtSession::MaxInputDimension(size_t index) const {
    if (index >= maximum_input_shape_.size() ||
        maximum_input_shape_[index] <= 0) {
        throw std::runtime_error(
            "TensorRT engine has an invalid maximum input profile shape");
    }
    return maximum_input_shape_[index];
}

void TensorRtSession::SetDevice() const {
    CheckCuda(cudaSetDevice(device_id_), "cudaSetDevice");
}

void TensorRtSession::EnsureDeviceBuffer(
    void** buffer, size_t& capacity, size_t bytes) {
    if (capacity >= bytes) return;
    if (*buffer != nullptr) CheckCuda(cudaFree(*buffer), "cudaFree");
    *buffer = nullptr;
    capacity = 0;
    CheckCuda(cudaMalloc(buffer, bytes), "cudaMalloc");
    capacity = bytes;
}

void TensorRtSession::EnsureHostBuffer(
    void** buffer, size_t& capacity, size_t bytes) {
    if (capacity >= bytes) return;
    if (*buffer != nullptr) CheckCuda(cudaFreeHost(*buffer), "cudaFreeHost");
    *buffer = nullptr;
    capacity = 0;
    CheckCuda(cudaHostAlloc(buffer, bytes, cudaHostAllocDefault), "cudaHostAlloc");
    capacity = bytes;
}

void TensorRtSession::ReleaseBuffers() noexcept {
    if (input_device_ != nullptr) cudaFree(input_device_);
    if (output_device_ != nullptr) cudaFree(output_device_);
    if (input_host_ != nullptr) cudaFreeHost(input_host_);
    if (output_host_ != nullptr) cudaFreeHost(output_host_);
    input_device_ = output_device_ = input_host_ = output_host_ = nullptr;
    input_device_capacity_ = output_device_capacity_ = 0;
    input_host_capacity_ = output_host_capacity_ = 0;
}

nvinfer1::Dims TensorRtSession::ToDims(const std::vector<int64_t>& shape) {
    if (shape.size() > nvinfer1::Dims::MAX_DIMS) {
        throw std::runtime_error("TensorRT input has too many dimensions");
    }
    nvinfer1::Dims dims{};
    dims.nbDims = static_cast<int32_t>(shape.size());
    for (int32_t index = 0; index < dims.nbDims; ++index) {
        const int64_t dimension = shape[static_cast<size_t>(index)];
        if (dimension <= 0 || dimension > std::numeric_limits<int32_t>::max()) {
            throw std::runtime_error("TensorRT input dimension is invalid");
        }
        dims.d[index] = static_cast<int32_t>(dimension);
    }
    return dims;
}

std::vector<int64_t> TensorRtSession::ToShape(const nvinfer1::Dims& dims) {
    std::vector<int64_t> shape;
    shape.reserve(static_cast<size_t>(dims.nbDims));
    for (int32_t index = 0; index < dims.nbDims; ++index) {
        shape.push_back(dims.d[index]);
    }
    return shape;
}

size_t TensorRtSession::Count(const std::vector<int64_t>& shape) {
    size_t count = 1;
    for (const int64_t dimension : shape) {
        if (dimension <= 0 ||
            count > std::numeric_limits<size_t>::max() /
                static_cast<size_t>(dimension)) {
            return 0;
        }
        count *= static_cast<size_t>(dimension);
    }
    return count;
}

}  // namespace lw::ppocr::tensorrt
