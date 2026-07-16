#include "tensorrt_engine.hpp"

#include "tensorrt_session.hpp"

#include <lw/ppocr/core/db_postprocessor.hpp>
#include <lw/ppocr/core/image_ops.hpp>
#include <lw/ppocr/core/model_manifest.hpp>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace lw::ppocr::tensorrt {
namespace {

using Clock = std::chrono::steady_clock;

double Milliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

int BucketWidth(int required, int minimum, int maximum) {
    constexpr int buckets[] = {320, 384, 448, 512, 640, 768, 960, 1152, 1280};
    for (const int bucket : buckets) {
        if (bucket >= minimum && bucket >= required && bucket <= maximum) {
            return bucket;
        }
    }
    return maximum;
}

class Detector {
public:
    Detector(TensorRtEnvironment& env, const core::ModelStage& model,
        const lw_ppocr_config& config)
        : session_(env, model.path, config.device_id),
          limit_side_len_(config.limit_side_len),
          threshold_(config.det_db_thresh),
          box_threshold_(config.det_db_box_thresh),
          unclip_ratio_(config.det_db_unclip_ratio),
          use_dilation_(config.det_use_dilation != 0) {}

    std::vector<core::OcrRegion> Run(
        const cv::Mat& image, core::StageTiming& timing) {
        const auto total_start = Clock::now();
        const auto preprocess_start = Clock::now();
        float ratio_h = 1.0f;
        float ratio_w = 1.0f;
        cv::Mat resized = core::ResizeForDetection(
            image, limit_side_len_, ratio_h, ratio_w);
        input_.resize(static_cast<size_t>(resized.rows) * resized.cols * 3);
        constexpr float mean[] = {0.485f, 0.456f, 0.406f};
        constexpr float scale[] = {
            1.0f / 0.229f, 1.0f / 0.224f, 1.0f / 0.225f};
        core::NormalizePermuteBgr(resized, input_.data(), mean, scale);
        const auto preprocess_end = Clock::now();

        const std::vector<int64_t> shape = {1, 3, resized.rows, resized.cols};
        const auto inference_start = Clock::now();
        TensorRtOutput output = session_.Run(input_.data(), input_.size(), shape);
        const auto inference_end = Clock::now();
        if (output.shape.size() < 2) {
            throw std::runtime_error("detector output tensor has an unsupported layout");
        }

        const auto postprocess_start = Clock::now();
        const int map_height = static_cast<int>(
            output.shape[output.shape.size() - 2]);
        const int map_width = static_cast<int>(output.shape.back());
        if (map_height <= 0 || map_width <= 0 ||
            output.ElementCount() < static_cast<size_t>(map_height) * map_width) {
            throw std::runtime_error("detector output tensor shape is invalid");
        }
        cv::Mat prediction(map_height, map_width, CV_32F,
            const_cast<float*>(output.Data()));
        cv::Mat bitmap;
        cv::compare(prediction, threshold_, bitmap, cv::CMP_GT);
        if (use_dilation_) {
            cv::dilate(bitmap, bitmap,
                cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2)));
        }
        const auto boxes = postprocessor_.Run(prediction, bitmap,
            box_threshold_, unclip_ratio_, "slow", ratio_h, ratio_w, image.size());
        std::vector<core::OcrRegion> regions;
        regions.reserve(boxes.size());
        for (const auto& box : boxes) {
            core::OcrRegion region;
            region.box = box;
            regions.push_back(std::move(region));
        }
        core::SortReadingOrder(regions);
        const auto postprocess_end = Clock::now();
        timing.preprocess_ms = Milliseconds(preprocess_start, preprocess_end);
        timing.inference_ms = Milliseconds(inference_start, inference_end);
        timing.postprocess_ms = Milliseconds(postprocess_start, postprocess_end);
        timing.total_ms = Milliseconds(total_start, postprocess_end);
        return regions;
    }

private:
    TensorRtSession session_;
    std::vector<float> input_;
    core::DbPostprocessor postprocessor_;
    int limit_side_len_;
    float threshold_;
    float box_threshold_;
    float unclip_ratio_;
    bool use_dilation_;
};

class Classifier {
public:
    Classifier(TensorRtEnvironment& env, const core::ModelStage& model,
        const lw_ppocr_config& config)
        : session_(env, model.path, config.device_id),
          height_(model.input_shape[2]), width_(model.input_shape[3]),
          batch_size_(config.cls_batch_size), threshold_(config.cls_threshold) {}

    void Run(std::vector<cv::Mat>& images,
        std::vector<core::OcrRegion>& regions, core::StageTiming& timing) {
        const auto total_start = Clock::now();
        constexpr float mean[] = {0.5f, 0.5f, 0.5f};
        constexpr float scale[] = {2.0f, 2.0f, 2.0f};
        for (size_t begin = 0; begin < images.size();
             begin += static_cast<size_t>(batch_size_)) {
            const size_t end = std::min(images.size(),
                begin + static_cast<size_t>(batch_size_));
            const auto preprocess_start = Clock::now();
            std::vector<cv::Mat> resized;
            for (size_t index = begin; index < end; ++index) {
                resized.push_back(core::ResizeForClassification(
                    images[index], height_, width_));
            }
            input_.resize(resized.size() * 3ULL * height_ * width_);
            core::NormalizePermuteBatchBgr(resized, input_.data(), mean, scale);
            const auto preprocess_end = Clock::now();
            const std::vector<int64_t> shape = {
                static_cast<int64_t>(resized.size()), 3, height_, width_};
            const auto inference_start = Clock::now();
            TensorRtOutput output = session_.Run(input_.data(), input_.size(), shape);
            const auto inference_end = Clock::now();
            if (output.shape.size() < 2 || output.shape[1] <= 0) {
                throw std::runtime_error("classifier output tensor shape is invalid");
            }
            const auto postprocess_start = Clock::now();
            const int count = std::min(static_cast<int>(resized.size()),
                static_cast<int>(output.shape[0]));
            const int classes = static_cast<int>(output.shape[1]);
            const float* values = output.Data();
            for (int batch = 0; batch < count; ++batch) {
                const float* row = values + static_cast<size_t>(batch) * classes;
                const float* maximum = std::max_element(row, row + classes);
                const size_t index = begin + static_cast<size_t>(batch);
                regions[index].cls_label = static_cast<int>(maximum - row);
                regions[index].cls_score = *maximum;
                if ((regions[index].cls_label % 2) == 1 && *maximum > threshold_) {
                    cv::rotate(images[index], images[index], cv::ROTATE_180);
                }
            }
            const auto postprocess_end = Clock::now();
            timing.preprocess_ms += Milliseconds(preprocess_start, preprocess_end);
            timing.inference_ms += Milliseconds(inference_start, inference_end);
            timing.postprocess_ms += Milliseconds(postprocess_start, postprocess_end);
        }
        timing.total_ms = Milliseconds(total_start, Clock::now());
    }

private:
    TensorRtSession session_;
    std::vector<float> input_;
    int height_;
    int width_;
    int batch_size_;
    float threshold_;
};

class Recognizer {
public:
    Recognizer(TensorRtEnvironment& env, const core::ModelStage& model,
        const std::filesystem::path& dictionary, const lw_ppocr_config& config)
        : labels_(core::ReadDictionary(core::PathToUtf8(dictionary))),
          height_(model.input_shape[2]), minimum_width_(model.input_shape[3]),
          batch_size_(config.rec_batch_size) {
        const int concurrency = env.RecognitionConcurrency(config.rec_concurrency);
        sessions_.reserve(static_cast<size_t>(concurrency));
        for (int index = 0; index < concurrency; ++index) {
            sessions_.push_back(std::make_unique<TensorRtSession>(
                env, model.path, config.device_id, true));
        }
        maximum_width_ = static_cast<int>(
            sessions_.front()->MaxInputDimension(3));
        if (maximum_width_ < minimum_width_) {
            throw std::runtime_error(
                "TensorRT recognition profile maximum width is smaller than "
                "the model manifest width");
        }
    }

    void Run(const std::vector<cv::Mat>& images,
        std::vector<core::OcrRegion>& regions, core::StageTiming& timing) {
        if (images.empty()) return;
        const auto total_start = Clock::now();
        std::vector<size_t> sorted(images.size());
        std::iota(sorted.begin(), sorted.end(), 0);
        std::sort(sorted.begin(), sorted.end(), [&images](size_t left, size_t right) {
            return static_cast<float>(images[left].cols) / images[left].rows <
                static_cast<float>(images[right].cols) / images[right].rows;
        });
        std::vector<std::vector<size_t>> batches;
        for (size_t begin = 0; begin < sorted.size();
             begin += static_cast<size_t>(batch_size_)) {
            const size_t end = std::min(sorted.size(),
                begin + static_cast<size_t>(batch_size_));
            batches.emplace_back(sorted.begin() + begin, sorted.begin() + end);
        }

        const size_t worker_count = std::min(sessions_.size(), batches.size());
        std::atomic<size_t> next_batch{0};
        std::vector<std::future<core::StageTiming>> workers;
        for (size_t worker = 0; worker < worker_count; ++worker) {
            workers.push_back(std::async(std::launch::async,
                [&, worker]() {
                    core::StageTiming worker_timing;
                    std::vector<float> input;
                    for (;;) {
                        const size_t batch = next_batch.fetch_add(1);
                        if (batch >= batches.size()) break;
                        RunBatch(*sessions_[worker], batches[batch], images,
                            regions, input, worker_timing);
                    }
                    return worker_timing;
                }));
        }
        for (auto& worker : workers) {
            const core::StageTiming value = worker.get();
            timing.preprocess_ms = std::max(timing.preprocess_ms, value.preprocess_ms);
            timing.inference_ms = std::max(timing.inference_ms, value.inference_ms);
            timing.postprocess_ms = std::max(timing.postprocess_ms, value.postprocess_ms);
        }
        timing.total_ms = Milliseconds(total_start, Clock::now());
    }

private:
    void RunBatch(TensorRtSession& session, const std::vector<size_t>& indices,
        const std::vector<cv::Mat>& images, std::vector<core::OcrRegion>& regions,
        std::vector<float>& input, core::StageTiming& timing) const {
        const auto preprocess_start = Clock::now();
        float maximum_ratio = static_cast<float>(minimum_width_) / height_;
        for (const size_t index : indices) {
            maximum_ratio = std::max(maximum_ratio,
                static_cast<float>(images[index].cols) / images[index].rows);
        }
        const int required_width = static_cast<int>(std::ceil(height_ * maximum_ratio));
        const int width = BucketWidth(
            required_width, minimum_width_, maximum_width_);
        std::vector<cv::Mat> resized;
        for (const size_t index : indices) {
            resized.push_back(core::ResizeForRecognition(images[index], height_, width));
        }
        input.resize(resized.size() * 3ULL * height_ * width);
        constexpr float mean[] = {0.5f, 0.5f, 0.5f};
        constexpr float scale[] = {2.0f, 2.0f, 2.0f};
        core::NormalizePermuteBatchBgr(resized, input.data(), mean, scale);
        const auto preprocess_end = Clock::now();
        const std::vector<int64_t> shape = {
            static_cast<int64_t>(indices.size()), 3, height_, width};
        const auto inference_start = Clock::now();
        TensorRtOutput output = session.Run(input.data(), input.size(), shape);
        const auto inference_end = Clock::now();
        if (output.shape.size() < 3 || output.shape[1] <= 0 || output.shape[2] <= 0) {
            throw std::runtime_error("recognizer output tensor shape is invalid");
        }
        const auto postprocess_start = Clock::now();
        const int count = std::min(static_cast<int>(indices.size()),
            static_cast<int>(output.shape[0]));
        const int steps = static_cast<int>(output.shape[1]);
        const int classes = static_cast<int>(output.shape[2]);
        const float* values = output.Data();
        const size_t batch_stride = static_cast<size_t>(steps) * classes;
        for (int batch = 0; batch < count; ++batch) {
            std::string text;
            float score_sum = 0.0f;
            int score_count = 0;
            int previous = -1;
            const float* batch_values = values + static_cast<size_t>(batch) * batch_stride;
            for (int step = 0; step < steps; ++step) {
                const float* row = batch_values + static_cast<size_t>(step) * classes;
                const float* maximum = std::max_element(row, row + classes);
                const int label = static_cast<int>(maximum - row);
                if (label > 0 && label != previous &&
                    label < static_cast<int>(labels_.size())) {
                    text += labels_[static_cast<size_t>(label)];
                    score_sum += *maximum;
                    ++score_count;
                }
                previous = label;
            }
            core::OcrRegion& region = regions[indices[static_cast<size_t>(batch)]];
            region.text = std::move(text);
            region.score = score_count > 0 ? score_sum / score_count : 0.0f;
        }
        const auto postprocess_end = Clock::now();
        timing.preprocess_ms += Milliseconds(preprocess_start, preprocess_end);
        timing.inference_ms += Milliseconds(inference_start, inference_end);
        timing.postprocess_ms += Milliseconds(postprocess_start, postprocess_end);
    }

    std::vector<std::unique_ptr<TensorRtSession>> sessions_;
    std::vector<std::string> labels_;
    int height_;
    int minimum_width_;
    int maximum_width_ = 0;
    int batch_size_;
};

}  // namespace

struct TensorRtOcrEngine::Impl {
    explicit Impl(const lw_ppocr_config& config)
        : log_level(config.log_level), log_callback(config.log_callback),
          log_user_data(config.log_user_data), enable_classifier(config.enable_cls != 0),
          environment(config),
          manifest(core::LoadModelManifest(config.model_manifest_utf8, "tensorrt")),
          detector(environment, manifest.detector, config),
          recognizer(environment, manifest.recognizer, manifest.dictionary, config) {
        if (enable_classifier) {
            if (!manifest.has_classifier) {
                throw std::runtime_error(
                    "classifier is enabled but the model manifest has no classifier stage");
            }
            classifier = std::make_unique<Classifier>(
                environment, manifest.classifier, config);
        }
    }

    lw_ppocr_log_level log_level;
    lw_ppocr_log_callback log_callback;
    void* log_user_data;
    bool enable_classifier;
    TensorRtEnvironment environment;
    core::ModelManifest manifest;
    Detector detector;
    std::unique_ptr<Classifier> classifier;
    Recognizer recognizer;
    std::mutex run_mutex;
};

TensorRtOcrEngine::TensorRtOcrEngine(const lw_ppocr_config& config)
    : impl_(std::make_unique<Impl>(config)) {
    Log(LW_PPOCR_LOG_INFO, "TensorRT Runtime loaded model package: " +
        impl_->manifest.name + ", GPU: " + std::to_string(config.device_id));
}

TensorRtOcrEngine::~TensorRtOcrEngine() = default;

core::PipelineResult TensorRtOcrEngine::Run(const lw_ppocr_image& image) {
    std::lock_guard<std::mutex> lock(impl_->run_mutex);
    const auto pipeline_start = Clock::now();
    const cv::Mat bgr = core::ToBgr(image);
    core::PipelineResult result;
    result.regions = impl_->detector.Run(bgr, result.detector);
    std::vector<cv::Mat> crops;
    std::vector<core::OcrRegion> valid_regions;
    for (core::OcrRegion& region : result.regions) {
        cv::Mat crop = core::GetRotateCropImage(bgr, region.box);
        if (!crop.empty()) {
            crops.push_back(std::move(crop));
            valid_regions.push_back(std::move(region));
        }
    }
    result.regions = std::move(valid_regions);
    if (impl_->classifier) {
        impl_->classifier->Run(crops, result.regions, result.classifier);
    }
    impl_->recognizer.Run(crops, result.regions, result.recognizer);
    result.pipeline.total_ms = Milliseconds(pipeline_start, Clock::now());
    std::ostringstream message;
    message << "TensorRT timing: det=" << result.detector.total_ms
            << " ms, cls=" << result.classifier.total_ms
            << " ms, rec=" << result.recognizer.total_ms
            << " ms, total=" << result.pipeline.total_ms << " ms";
    Log(LW_PPOCR_LOG_DEBUG, message.str());
    return result;
}

core::RecognitionResult TensorRtOcrEngine::RecognizeBatch(
    const lw_ppocr_image* images, uint64_t image_count) {
    std::lock_guard<std::mutex> lock(impl_->run_mutex);
    const Clock::time_point pipeline_start = Clock::now();
    std::vector<cv::Mat> crops;
    crops.reserve(static_cast<size_t>(image_count));
    for (uint64_t index = 0; index < image_count; ++index) {
        crops.push_back(core::ToBgr(images[index]).clone());
    }

    core::RecognitionResult result;
    result.items.resize(crops.size());
    if (impl_->classifier != nullptr) {
        impl_->classifier->Run(crops, result.items, result.classifier);
    }
    impl_->recognizer.Run(crops, result.items, result.recognizer);
    result.pipeline.total_ms = Milliseconds(pipeline_start, Clock::now());
    return result;
}

void TensorRtOcrEngine::Log(
    lw_ppocr_log_level level, const std::string& message) const noexcept {
    if (!impl_ || !impl_->log_callback || level > impl_->log_level) return;
    try {
        impl_->log_callback(level, message.c_str(), impl_->log_user_data);
    } catch (...) {
    }
}

}  // namespace lw::ppocr::tensorrt
