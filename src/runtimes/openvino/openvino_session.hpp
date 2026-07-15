#pragma once

#include <lw/ppocr/ppocr_api.h>

#include <openvino/openvino.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace lw::ppocr::openvino {

struct OpenVinoOutput {
    ov::Tensor tensor;
    std::vector<int64_t> shape;

    const float* Data() const;
    size_t ElementCount() const;
};

class OpenVinoEnvironment {
public:
    explicit OpenVinoEnvironment(const lw_ppocr_config& config);

    std::shared_ptr<ov::CompiledModel> Compile(
        const std::filesystem::path& model_path,
        bool recognition_model);
    const std::string& Device() const noexcept;
    int RecognitionConcurrency(int requested) const noexcept;

private:
    ov::Core core_;
    std::string device_;
    int recognition_requests_ = 1;
    std::mutex compile_mutex_;
    std::map<std::filesystem::path, std::shared_ptr<ov::CompiledModel>> models_;
};

class OpenVinoSession {
public:
    OpenVinoSession(OpenVinoEnvironment& environment,
        const std::filesystem::path& model_path,
        int device_id,
        bool recognition_model = false);

    OpenVinoOutput Run(
        float* input,
        size_t element_count,
        const std::vector<int64_t>& shape);

private:
    std::shared_ptr<ov::CompiledModel> compiled_model_;
    ov::InferRequest request_;
};

}  // namespace lw::ppocr::openvino
