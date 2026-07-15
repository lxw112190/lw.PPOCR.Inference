#include "openvino_session.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace lw::ppocr::openvino {
namespace {

std::string SelectDevice(const lw_ppocr_config& config) {
    std::string options = config.backend_options_json_utf8 != nullptr
        ? config.backend_options_json_utf8
        : "";
    options.erase(std::remove_if(options.begin(), options.end(),
        [](unsigned char value) { return std::isspace(value) != 0; }), options.end());
    std::transform(options.begin(), options.end(), options.begin(),
        [](unsigned char value) { return static_cast<char>(std::toupper(value)); });

    std::string device = "CPU";
    const std::string key = "\"DEVICE\":\"";
    const size_t begin = options.find(key);
    if (begin != std::string::npos) {
        const size_t value_begin = begin + key.size();
        const size_t value_end = options.find('"', value_begin);
        if (value_end == std::string::npos) {
            throw std::invalid_argument("OpenVINO backend option 'device' is malformed");
        }
        device = options.substr(value_begin, value_end - value_begin);
    } else if (options == "CPU" || options == "GPU" || options == "AUTO") {
        device = options;
    }

    if (device != "CPU") {
        throw std::invalid_argument(
            "the stable OpenVINO Runtime currently supports CPU only; use DirectML for GPU inference");
    }
    return device;
}

}  // namespace

const float* OpenVinoOutput::Data() const {
    return tensor.data<const float>();
}

size_t OpenVinoOutput::ElementCount() const {
    return tensor.get_size();
}

OpenVinoEnvironment::OpenVinoEnvironment(const lw_ppocr_config& config)
    : device_(SelectDevice(config)),
      recognition_requests_(RecognitionConcurrency(config.rec_concurrency)) {}

std::shared_ptr<ov::CompiledModel> OpenVinoEnvironment::Compile(
    const std::filesystem::path& model_path,
    bool recognition_model) {
    std::lock_guard<std::mutex> lock(compile_mutex_);
    const std::filesystem::path key = std::filesystem::absolute(model_path).lexically_normal();
    const auto existing = models_.find(key);
    if (existing != models_.end()) {
        return existing->second;
    }
    auto model = std::make_shared<ov::CompiledModel>(
        recognition_model && device_ == "CPU"
            ? core_.compile_model(key.wstring(), device_,
                ov::hint::performance_mode(ov::hint::PerformanceMode::THROUGHPUT),
                ov::hint::num_requests(recognition_requests_))
            : core_.compile_model(key.wstring(), device_));
    models_.emplace(key, model);
    return model;
}

const std::string& OpenVinoEnvironment::Device() const noexcept {
    return device_;
}

int OpenVinoEnvironment::RecognitionConcurrency(int requested) const noexcept {
    return std::max(1, std::min(requested, 8));
}

OpenVinoSession::OpenVinoSession(
    OpenVinoEnvironment& environment,
    const std::filesystem::path& model_path,
    int,
    bool recognition_model)
    : compiled_model_(environment.Compile(model_path, recognition_model)),
      request_(compiled_model_->create_infer_request()) {}

OpenVinoOutput OpenVinoSession::Run(
    float* input,
    size_t element_count,
    const std::vector<int64_t>& shape) {
    ov::Shape input_shape;
    input_shape.reserve(shape.size());
    size_t expected = 1;
    for (const int64_t dimension : shape) {
        if (dimension <= 0) {
            throw std::invalid_argument("OpenVINO input shape contains a non-positive dimension");
        }
        input_shape.push_back(static_cast<size_t>(dimension));
        expected *= static_cast<size_t>(dimension);
    }
    if (expected != element_count) {
        throw std::invalid_argument("OpenVINO input element count does not match its shape");
    }

    ov::Tensor input_tensor(ov::element::f32, input_shape, input);
    request_.set_input_tensor(input_tensor);
    request_.infer();
    OpenVinoOutput output;
    output.tensor = request_.get_output_tensor(0);
    if (output.tensor.get_element_type() != ov::element::f32) {
        throw std::runtime_error("OpenVINO output tensor is not FP32");
    }
    for (const size_t dimension : output.tensor.get_shape()) {
        output.shape.push_back(static_cast<int64_t>(dimension));
    }
    return output;
}

}  // namespace lw::ppocr::openvino
