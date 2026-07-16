#include <lw/ppocr/ppocr_api.h>

#include <opencv2/imgcodecs.hpp>

#include <atomic>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

namespace {

struct ProcessSnapshot {
    uint64_t private_bytes = 0;
    uint32_t handles = 0;
};

int Fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

int EnvironmentInteger(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    try {
        const int parsed = std::stoi(value);
        return parsed > 0 ? parsed : 0;
    } catch (...) {
        return fallback;
    }
}

ProcessSnapshot Snapshot() {
    ProcessSnapshot snapshot{};
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)) != FALSE) {
        snapshot.private_bytes = counters.PrivateUsage;
    }
    DWORD handles = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &handles) != FALSE) {
        snapshot.handles = handles;
    }
#endif
    return snapshot;
}

bool RunAndValidate(
    lw_ppocr_handle engine,
    const lw_ppocr_image& image,
    uint64_t expected_region_count) {
    lw_ppocr_result* result = nullptr;
    const lw_ppocr_status status = lw_ppocr_run(engine, &image, &result);
    if (status != LW_PPOCR_STATUS_OK || result == nullptr ||
        result->region_count != expected_region_count) {
        if (result != nullptr) {
            lw_ppocr_result_free(engine, result);
        }
        return false;
    }
    lw_ppocr_result_free(engine, result);
    return true;
}

std::string LastError(lw_ppocr_handle handle) {
    const uint64_t required = lw_ppocr_get_last_error(handle, nullptr, 0);
    std::vector<char> buffer(static_cast<size_t>(required > 0 ? required : 1));
    lw_ppocr_get_last_error(handle, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

std::string JsonPath(const std::filesystem::path& path) {
    std::string value = std::filesystem::absolute(path).generic_u8string();
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

void WriteManifest(
    const std::filesystem::path& path,
    const std::filesystem::path& detector,
    const std::filesystem::path& recognizer,
    const std::filesystem::path& dictionary,
    const std::filesystem::path& classifier,
    const std::string& artifact_key = "onnx") {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot create temporary model manifest");
    }
    output
        << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"name\": \"PP-OCR integration test\",\n"
        << "  \"family\": \"PP-OCR\",\n"
        << "  \"language\": \"ch\",\n"
        << "  \"dictionary\": {\"path\": \"" << JsonPath(dictionary)
        << "\"},\n"
        << "  \"stages\": {\n"
        << "    \"detector\": {\"input_shape\": [-1, 3, -1, -1], "
        << "\"artifacts\": {\"" << artifact_key << "\": {\"path\": \""
        << JsonPath(detector)
        << "\", \"format\": \"" << (artifact_key == "tensorrt" ? "tensorrt-engine" : "onnx")
        << "\", \"precision\": \"" << (artifact_key == "tensorrt" ? "fp16" : "fp32") << "\"}}},\n"
        << "    \"recognizer\": {\"input_shape\": [1, 3, 48, 320], "
        << "\"artifacts\": {\"" << artifact_key << "\": {\"path\": \""
        << JsonPath(recognizer)
        << "\", \"format\": \"" << (artifact_key == "tensorrt" ? "tensorrt-engine" : "onnx")
        << "\", \"precision\": \"" << (artifact_key == "tensorrt" ? "fp16" : "fp32") << "\"}}}";
    if (!classifier.empty()) {
        output
            << ",\n    \"classifier\": {\"input_shape\": [1, 3, 80, 160], "
            << "\"artifacts\": {\"" << artifact_key << "\": {\"path\": \""
            << JsonPath(classifier)
            << "\", \"format\": \"" << (artifact_key == "tensorrt" ? "tensorrt-engine" : "onnx")
            << "\", \"precision\": \"" << (artifact_key == "tensorrt" ? "fp16" : "fp32") << "\"}}}";
    }
    output
        << "\n"
        << "  }\n"
        << "}\n";
}

void LW_PPOCR_CALL LogCallback(
    lw_ppocr_log_level level,
    const char* message_utf8,
    void*) {
    if (message_utf8 != nullptr) {
        std::cout << "[runtime " << static_cast<int>(level) << "] "
                  << message_utf8 << '\n';
    }
}

void PrintTiming(const char* name, const lw_ppocr_timing& timing) {
    std::cout << std::fixed << std::setprecision(3)
              << name << " timing: preprocess=" << timing.preprocess_ms
              << " ms, inference=" << timing.inference_ms
              << " ms, postprocess=" << timing.postprocess_ms
              << " ms, total=" << timing.total_ms << " ms\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 6 && argc != 8 && argc != 9) {
        return Fail(
            "expected: runtime.dll detector recognizer dictionary.txt image [directml|openvino|tensorrt device classifier]");
    }

    try {
        const std::filesystem::path manifest =
            std::filesystem::temp_directory_path() /
            "lw_ppocr_integration_manifest.json";
        const std::filesystem::path classifier = argc == 9
            ? std::filesystem::path(argv[8])
            : std::filesystem::path();
        const std::string runtime_name = argc >= 8 ? argv[6] : "opencv";
        const bool use_directml = runtime_name == "directml";
        const bool use_onnxruntime = runtime_name == "onnxruntime";
        const bool use_openvino = runtime_name == "openvino";
        const bool use_tensorrt = runtime_name == "tensorrt";
        WriteManifest(manifest, argv[2], argv[3], argv[4], classifier,
            use_tensorrt ? "tensorrt" : "onnx");

        const cv::Mat source = cv::imread(argv[5], cv::IMREAD_COLOR);
        if (source.empty()) {
            std::filesystem::remove(manifest);
            return Fail("failed to load test image");
        }

        const std::string manifest_utf8 = manifest.u8string();
        lw_ppocr_config config{};
        config.struct_size = sizeof(config);
        config.api_version = LW_PPOCR_API_VERSION;
        const lw_ppocr_backend backend = use_directml
            ? LW_PPOCR_BACKEND_DIRECTML
            : (use_onnxruntime ? LW_PPOCR_BACKEND_ONNXRUNTIME
                : (use_openvino ? LW_PPOCR_BACKEND_OPENVINO
                                : (use_tensorrt ? LW_PPOCR_BACKEND_TENSORRT
                                               : LW_PPOCR_BACKEND_OPENCV_DNN)));
        const std::string backend_options = use_openvino
            ? std::string("{\"device\":\"") + argv[7] + "\"}"
            : (use_onnxruntime
                ? std::string("{\"device\":\"") + argv[7] + "\"}"
                               : std::string());
        config.backend = backend;
        config.device_id = use_directml || use_tensorrt ? std::stoi(argv[7]) : 0;
        config.backend_options_json_utf8 = backend_options.empty()
            ? nullptr
            : backend_options.c_str();
        config.runtime_library_utf8 = argv[1];
        config.model_manifest_utf8 = manifest_utf8.c_str();
        config.enable_cls = classifier.empty() ? 0 : 1;
        config.limit_side_len = 960;
        config.det_db_thresh = 0.3f;
        config.det_db_box_thresh = 0.6f;
        config.det_db_unclip_ratio = 1.6f;
        config.det_use_dilation = 0;
        config.cls_threshold = 0.9f;
        config.cls_batch_size = 8;
        config.rec_batch_size = use_openvino ? 1 : 4;
        config.rec_concurrency = use_openvino ? 8
            : ((use_directml || use_onnxruntime || use_tensorrt) ? 4 : 2);
        config.log_level = LW_PPOCR_LOG_INFO;
        config.log_callback = &LogCallback;

        lw_ppocr_handle engine = nullptr;
        const lw_ppocr_status create_status = lw_ppocr_create(&config, &engine);
        if (create_status != LW_PPOCR_STATUS_OK || engine == nullptr) {
            const std::string error = LastError(nullptr);
            std::filesystem::remove(manifest);
            return Fail("create failed: " + error);
        }

        lw_ppocr_capabilities capabilities{};
        capabilities.struct_size = sizeof(capabilities);
        if (lw_ppocr_get_capabilities(engine, &capabilities) !=
                LW_PPOCR_STATUS_OK ||
            capabilities.backend != backend ||
            ((use_directml || use_tensorrt)
                ? capabilities.supports_gpu != 1
                : capabilities.supports_cpu != 1)) {
            const std::string error = LastError(engine);
            lw_ppocr_destroy(&engine);
            std::filesystem::remove(manifest);
            return Fail("capability query failed: " + error);
        }

        lw_ppocr_image image{};
        image.struct_size = sizeof(image);
        image.api_version = LW_PPOCR_API_VERSION;
        image.data = source.data;
        image.data_size = static_cast<uint64_t>(source.step) * source.rows;
        image.width = source.cols;
        image.height = source.rows;
        image.stride = static_cast<int32_t>(source.step);
        image.pixel_format = LW_PPOCR_PIXEL_FORMAT_BGR24;

        if (use_directml || use_openvino || use_tensorrt) {
            const int warmup_count = 8;
            for (int iteration = 0; iteration < warmup_count; ++iteration) {
                lw_ppocr_result* warmup = nullptr;
                const lw_ppocr_status warmup_status =
                    lw_ppocr_run(engine, &image, &warmup);
                if (warmup_status != LW_PPOCR_STATUS_OK || warmup == nullptr) {
                    const std::string error = LastError(engine);
                    lw_ppocr_destroy(&engine);
                    std::filesystem::remove(manifest);
                    return Fail("accelerated Runtime warmup failed: " + error);
                }
                lw_ppocr_result_free(engine, warmup);
            }
        }

        lw_ppocr_result* result = nullptr;
        const lw_ppocr_status run_status = lw_ppocr_run(engine, &image, &result);
        if (run_status != LW_PPOCR_STATUS_OK || result == nullptr) {
            const std::string error = LastError(engine);
            lw_ppocr_destroy(&engine);
            std::filesystem::remove(manifest);
            return Fail("inference failed: " + error);
        }
        if (result->region_count == 0) {
            lw_ppocr_result_free(engine, result);
            lw_ppocr_destroy(&engine);
            std::filesystem::remove(manifest);
            return Fail("inference returned no text regions");
        }

        std::cout << "Recognized " << result->region_count << " regions:\n";
        for (uint64_t index = 0; index < result->region_count; ++index) {
            const lw_ppocr_text_region& region = result->regions[index];
            std::cout << "  [" << index << "] "
                      << (region.text_utf8 != nullptr ? region.text_utf8 : "")
                      << " (score=" << std::fixed << std::setprecision(4)
                      << region.score << ")\n";
        }
        PrintTiming("det", result->detector);
        if (!classifier.empty()) {
            PrintTiming("cls", result->classifier);
        }
        PrintTiming("rec", result->recognizer);
        PrintTiming("pipeline", result->pipeline);

        const uint64_t expected_region_count = result->region_count;
        lw_ppocr_result_free(engine, result);

        lw_ppocr_image crops[2] = {image, image};
        lw_ppocr_recognition_result* recognition = nullptr;
        const lw_ppocr_status recognition_status = lw_ppocr_recognize_batch(
            engine, crops, 2, &recognition);
        if (recognition_status != LW_PPOCR_STATUS_OK || recognition == nullptr ||
            recognition->item_count != 2 || recognition->items == nullptr ||
            recognition->items[0].source_index != 0 ||
            recognition->items[1].source_index != 1) {
            const std::string error = LastError(engine);
            lw_ppocr_recognition_result_free(engine, recognition);
            lw_ppocr_destroy(&engine);
            std::filesystem::remove(manifest);
            return Fail("recognition-only batch failed: " + error);
        }
        PrintTiming("recognition-only rec", recognition->recognizer);
        PrintTiming("recognition-only pipeline", recognition->pipeline);
        lw_ppocr_recognition_result_free(engine, recognition);

        const int stress_iterations =
            EnvironmentInteger("LW_PPOCR_STRESS_ITERATIONS", 0);
        const int stress_threads =
            EnvironmentInteger("LW_PPOCR_STRESS_THREADS", 0);
        const int stress_runs_per_thread =
            EnvironmentInteger("LW_PPOCR_STRESS_RUNS_PER_THREAD", 10);
        const ProcessSnapshot stress_baseline = Snapshot();

        for (int iteration = 0; iteration < stress_iterations; ++iteration) {
            if (!RunAndValidate(engine, image, expected_region_count)) {
                const std::string error = LastError(engine);
                lw_ppocr_destroy(&engine);
                std::filesystem::remove(manifest);
                return Fail("long-running inference failed at iteration " +
                    std::to_string(iteration) + ": " + error);
            }
        }

        std::atomic<int> thread_failures{0};
        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(stress_threads));
        for (int thread_index = 0; thread_index < stress_threads;
             ++thread_index) {
            workers.emplace_back([&]() {
                for (int iteration = 0; iteration < stress_runs_per_thread;
                     ++iteration) {
                    if (!RunAndValidate(engine, image, expected_region_count)) {
                        ++thread_failures;
                        return;
                    }
                }
            });
        }
        for (std::thread& worker : workers) {
            worker.join();
        }
        if (thread_failures.load() != 0) {
            const std::string error = LastError(engine);
            lw_ppocr_destroy(&engine);
            std::filesystem::remove(manifest);
            return Fail("concurrent inference failed: " + error);
        }

        const ProcessSnapshot stress_completed = Snapshot();
        constexpr uint64_t kMaxPrivateGrowth = 256ULL * 1024ULL * 1024ULL;
        constexpr uint32_t kMaxHandleGrowth = 32;
        if (stress_baseline.private_bytes != 0 &&
            stress_completed.private_bytes >
                stress_baseline.private_bytes + kMaxPrivateGrowth) {
            lw_ppocr_destroy(&engine);
            std::filesystem::remove(manifest);
            return Fail("private memory grew by more than 256 MiB");
        }
        if (stress_baseline.handles != 0 && stress_completed.handles >
                stress_baseline.handles + kMaxHandleGrowth) {
            lw_ppocr_destroy(&engine);
            std::filesystem::remove(manifest);
            return Fail("process handle count grew unexpectedly");
        }
        if (stress_iterations > 0 || stress_threads > 0) {
            std::cout << "Stability stress passed: sequential_runs="
                      << stress_iterations << ", concurrent_runs="
                      << stress_threads * stress_runs_per_thread
                      << ", private_growth="
                      << (stress_completed.private_bytes >=
                                  stress_baseline.private_bytes
                              ? stress_completed.private_bytes -
                                    stress_baseline.private_bytes
                              : 0)
                      << " bytes, handle_growth="
                      << (stress_completed.handles >= stress_baseline.handles
                              ? stress_completed.handles -
                                    stress_baseline.handles
                              : 0)
                      << '\n';
        }

        lw_ppocr_destroy(&engine);
        std::filesystem::remove(manifest);
        if (engine != nullptr) {
            return Fail("destroy did not clear the engine handle");
        }
        std::cout << (use_directml ? "DirectML"
            : (use_openvino ? "OpenVINO"
                            : (use_tensorrt ? "TensorRT"
                                : (use_onnxruntime ? "ONNX Runtime" : "OpenCV DNN"))))
                  << " integration test passed\n";
        return 0;
    } catch (const std::exception& exception) {
        return Fail(exception.what());
    }
}
