#pragma once

#include <onnxruntime_cxx_api.h>

#include <filesystem>
#include <string>
#include <vector>

namespace lw::ppocr::directml {

struct OrtOutput {
    std::vector<Ort::Value> values;
    std::vector<int64_t> shape;

    const float* Data() const;
    size_t ElementCount() const;
};

class OrtSession {
public:
    OrtSession(Ort::Env& environment,
        const std::filesystem::path& model_path,
        int device_id,
        bool use_gpu);

    OrtOutput Run(
        float* input,
        size_t element_count,
        const std::vector<int64_t>& shape);

private:
    Ort::Session session_{nullptr};
    std::vector<std::string> input_name_storage_;
    std::vector<std::string> output_name_storage_;
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
};

}  // namespace lw::ppocr::directml
