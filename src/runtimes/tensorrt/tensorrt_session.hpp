#pragma once

#include <lw/ppocr/ppocr_api.h>

#include <NvInfer.h>
#include <cuda_runtime_api.h>

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace lw::ppocr::tensorrt {

struct TensorRtOutput {
    const float* data = nullptr;
    std::vector<int64_t> shape;
    size_t element_count = 0;

    const float* Data() const noexcept { return data; }
    size_t ElementCount() const noexcept { return element_count; }
};

class TensorRtLogger final : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* message) noexcept override;
};

class TensorRtEnvironment {
public:
    explicit TensorRtEnvironment(const lw_ppocr_config& config);

    std::shared_ptr<nvinfer1::ICudaEngine> LoadEngine(
        const std::filesystem::path& path);
    int DeviceId() const noexcept { return device_id_; }
    int RecognitionConcurrency(int requested) const noexcept;

private:
    int device_id_ = 0;
    TensorRtLogger logger_;
    std::unique_ptr<nvinfer1::IRuntime> runtime_;
    std::mutex load_mutex_;
    std::map<std::filesystem::path,
        std::shared_ptr<nvinfer1::ICudaEngine>> engines_;
};

class TensorRtSession {
public:
    TensorRtSession(TensorRtEnvironment& environment,
        const std::filesystem::path& engine_path,
        int device_id,
        bool recognition_model = false);
    ~TensorRtSession();

    TensorRtSession(const TensorRtSession&) = delete;
    TensorRtSession& operator=(const TensorRtSession&) = delete;

    TensorRtOutput Run(float* input,
        size_t element_count,
        const std::vector<int64_t>& shape);
    int64_t MaxInputDimension(size_t index) const;

private:
    void SetDevice() const;
    void EnsureDeviceBuffer(void** buffer, size_t& capacity, size_t bytes);
    void EnsureHostBuffer(void** buffer, size_t& capacity, size_t bytes);
    void ReleaseBuffers() noexcept;
    static nvinfer1::Dims ToDims(const std::vector<int64_t>& shape);
    static std::vector<int64_t> ToShape(const nvinfer1::Dims& dims);
    static size_t Count(const std::vector<int64_t>& shape);

    int device_id_ = 0;
    std::shared_ptr<nvinfer1::ICudaEngine> engine_;
    std::unique_ptr<nvinfer1::IExecutionContext> context_;
    cudaStream_t stream_ = nullptr;
    std::string input_name_;
    std::string output_name_;
    std::vector<int64_t> maximum_input_shape_;
    void* input_device_ = nullptr;
    void* output_device_ = nullptr;
    void* input_host_ = nullptr;
    void* output_host_ = nullptr;
    size_t input_device_capacity_ = 0;
    size_t output_device_capacity_ = 0;
    size_t input_host_capacity_ = 0;
    size_t output_host_capacity_ = 0;
};

}  // namespace lw::ppocr::tensorrt
