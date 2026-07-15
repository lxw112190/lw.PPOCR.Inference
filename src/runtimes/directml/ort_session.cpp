#include "ort_session.hpp"

#include <dml_provider_factory.h>

#include <stdexcept>

namespace lw::ppocr::directml {

const float* OrtOutput::Data() const {
    return values.front().GetTensorData<float>();
}

size_t OrtOutput::ElementCount() const {
    return values.front().GetTensorTypeAndShapeInfo().GetElementCount();
}

OrtSession::OrtSession(
    Ort::Env& environment,
    const std::filesystem::path& model_path,
    int device_id,
    bool use_gpu) {
    Ort::SessionOptions options;
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    if (use_gpu) {
        options.DisableMemPattern();
        Ort::ThrowOnError(
            OrtSessionOptionsAppendExecutionProvider_DML(options, device_id));
    }

    session_ = Ort::Session(environment, model_path.c_str(), options);
    Ort::AllocatorWithDefaultOptions allocator;
    for (size_t index = 0; index < session_.GetInputCount(); ++index) {
        input_name_storage_.emplace_back(
            session_.GetInputNameAllocated(index, allocator).get());
    }
    for (size_t index = 0; index < session_.GetOutputCount(); ++index) {
        output_name_storage_.emplace_back(
            session_.GetOutputNameAllocated(index, allocator).get());
    }
    if (input_name_storage_.empty() || output_name_storage_.empty()) {
        throw std::runtime_error("ONNX model has no input or output tensor");
    }
    for (const std::string& name : input_name_storage_) {
        input_names_.push_back(name.c_str());
    }
    for (const std::string& name : output_name_storage_) {
        output_names_.push_back(name.c_str());
    }
}

OrtOutput OrtSession::Run(
    float* input,
    size_t element_count,
    const std::vector<int64_t>& shape) {
    Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tensor = Ort::Value::CreateTensor<float>(
        memory, input, element_count, shape.data(), shape.size());

    OrtOutput output;
    output.values = session_.Run(Ort::RunOptions{nullptr},
        input_names_.data(), &tensor, 1,
        output_names_.data(), output_names_.size());
    if (output.values.empty() || !output.values.front().IsTensor()) {
        throw std::runtime_error("ONNX Runtime returned no tensor output");
    }
    const auto info = output.values.front().GetTensorTypeAndShapeInfo();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
        throw std::runtime_error("ONNX Runtime output is not FP32");
    }
    output.shape = info.GetShape();
    return output;
}

}  // namespace lw::ppocr::directml
