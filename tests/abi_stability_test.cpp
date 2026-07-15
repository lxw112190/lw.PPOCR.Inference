#include <lw/ppocr/ppocr_api.h>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

namespace {

constexpr int kLifecycleIterations = 1000;
constexpr int kRunIterations = 10000;
constexpr int kThreadCount = 8;
constexpr int kRunsPerThread = 1000;

struct ProcessSnapshot {
    uint64_t private_bytes = 0;
    uint32_t handles = 0;
};

int Fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    return 1;
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

lw_ppocr_config MakeConfig(const char* runtime_path) {
    lw_ppocr_config config{};
    config.struct_size = sizeof(config);
    config.api_version = LW_PPOCR_API_VERSION;
    config.backend = LW_PPOCR_BACKEND_OPENCV_DNN;
    config.runtime_library_utf8 = runtime_path;
    config.model_manifest_utf8 = "stub://stability";
    config.limit_side_len = 960;
    config.det_db_thresh = 0.3f;
    config.det_db_box_thresh = 0.6f;
    config.det_db_unclip_ratio = 1.6f;
    config.cls_threshold = 0.9f;
    config.cls_batch_size = 8;
    config.rec_batch_size = 1;
    config.rec_concurrency = 1;
    config.log_level = LW_PPOCR_LOG_OFF;
    return config;
}

lw_ppocr_image MakeImage(unsigned char* pixels, uint64_t size) {
    lw_ppocr_image image{};
    image.struct_size = sizeof(image);
    image.api_version = LW_PPOCR_API_VERSION;
    image.data = pixels;
    image.data_size = size;
    image.width = 2;
    image.height = 2;
    image.stride = 6;
    image.pixel_format = LW_PPOCR_PIXEL_FORMAT_BGR24;
    return image;
}

bool RunOnce(lw_ppocr_handle engine, const lw_ppocr_image& image) {
    lw_ppocr_result* result = nullptr;
    const lw_ppocr_status status = lw_ppocr_run(engine, &image, &result);
    if (status != LW_PPOCR_STATUS_OK || result == nullptr ||
        result->region_count != 0) {
        return false;
    }
    lw_ppocr_result_free(engine, result);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return Fail("expected the Stub Runtime path");
    }

    unsigned char pixels[12] = {};
    const lw_ppocr_image image = MakeImage(pixels, sizeof(pixels));
    const lw_ppocr_config config = MakeConfig(argv[1]);
    const ProcessSnapshot process_start = Snapshot();

    for (int iteration = 0; iteration < kLifecycleIterations; ++iteration) {
        lw_ppocr_handle engine = nullptr;
        if (lw_ppocr_create(&config, &engine) != LW_PPOCR_STATUS_OK ||
            engine == nullptr || !RunOnce(engine, image)) {
            lw_ppocr_destroy(&engine);
            return Fail("create/run failed during lifecycle iteration " +
                std::to_string(iteration));
        }
        lw_ppocr_destroy(&engine);
        if (engine != nullptr) {
            return Fail("destroy did not clear the lifecycle handle");
        }
    }

    lw_ppocr_handle engine = nullptr;
    if (lw_ppocr_create(&config, &engine) != LW_PPOCR_STATUS_OK ||
        engine == nullptr) {
        return Fail("persistent engine creation failed");
    }

    for (int iteration = 0; iteration < 100; ++iteration) {
        if (!RunOnce(engine, image)) {
            lw_ppocr_destroy(&engine);
            return Fail("warmup failed");
        }
    }
    const ProcessSnapshot baseline = Snapshot();

    for (int iteration = 0; iteration < kRunIterations; ++iteration) {
        if (!RunOnce(engine, image)) {
            lw_ppocr_destroy(&engine);
            return Fail("long-running call failed at iteration " +
                std::to_string(iteration));
        }
        if (iteration % 100 == 0) {
            char* json = nullptr;
            uint64_t json_length = 0;
            if (lw_ppocr_run_json(engine, &image, &json, &json_length) !=
                    LW_PPOCR_STATUS_OK ||
                json == nullptr || json_length != 2) {
                lw_ppocr_destroy(&engine);
                return Fail("JSON allocation stress failed");
            }
            lw_ppocr_string_free(engine, json);
        }
    }

    std::atomic<int> thread_failures{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for (int thread_index = 0; thread_index < kThreadCount; ++thread_index) {
        workers.emplace_back([&]() {
            for (int iteration = 0; iteration < kRunsPerThread; ++iteration) {
                if (!RunOnce(engine, image)) {
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
        lw_ppocr_destroy(&engine);
        return Fail("concurrent calls failed");
    }

    std::vector<lw_ppocr_handle> engines(kThreadCount, nullptr);
    for (lw_ppocr_handle& instance : engines) {
        if (lw_ppocr_create(&config, &instance) != LW_PPOCR_STATUS_OK ||
            instance == nullptr) {
            for (lw_ppocr_handle& created : engines) {
                lw_ppocr_destroy(&created);
            }
            lw_ppocr_destroy(&engine);
            return Fail("multiple-instance creation failed");
        }
    }
    workers.clear();
    thread_failures = 0;
    for (lw_ppocr_handle instance : engines) {
        workers.emplace_back([&, instance]() {
            for (int iteration = 0; iteration < 100; ++iteration) {
                if (!RunOnce(instance, image)) {
                    ++thread_failures;
                    return;
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    for (lw_ppocr_handle& instance : engines) {
        lw_ppocr_destroy(&instance);
    }
    if (thread_failures.load() != 0) {
        lw_ppocr_destroy(&engine);
        return Fail("multiple-instance calls failed");
    }

    lw_ppocr_image invalid_image = image;
    invalid_image.data_size = 1;
    lw_ppocr_result* invalid_result = nullptr;
    if (lw_ppocr_run(engine, &invalid_image, &invalid_result) !=
            LW_PPOCR_STATUS_INVALID_ARGUMENT ||
        invalid_result != nullptr) {
        lw_ppocr_destroy(&engine);
        return Fail("malformed image was not rejected");
    }

    const ProcessSnapshot completed = Snapshot();
    lw_ppocr_destroy(&engine);
    if (engine != nullptr) {
        return Fail("destroy did not clear the persistent handle");
    }
    const ProcessSnapshot process_end = Snapshot();

    constexpr uint64_t kMaxPrivateGrowth = 16ULL * 1024ULL * 1024ULL;
    constexpr uint32_t kMaxHandleGrowth = 8;
    if (baseline.private_bytes != 0 && completed.private_bytes >
            baseline.private_bytes + kMaxPrivateGrowth) {
        return Fail("private memory grew by more than 16 MiB");
    }
    if (baseline.handles != 0 && completed.handles >
            baseline.handles + kMaxHandleGrowth) {
        return Fail("process handle count grew unexpectedly");
    }
    if (process_start.handles != 0 && process_end.handles >
            process_start.handles + kMaxHandleGrowth) {
        return Fail("lifecycle test leaked process handles");
    }

    std::cout << "ABI stability passed: lifecycle=" << kLifecycleIterations
              << ", sequential_runs=" << kRunIterations
              << ", concurrent_runs=" << kThreadCount * kRunsPerThread
              << ", parallel_instances=" << kThreadCount
              << ", private_growth="
              << (completed.private_bytes >= baseline.private_bytes
                      ? completed.private_bytes - baseline.private_bytes
                      : 0)
              << " bytes, handle_growth="
              << (completed.handles >= baseline.handles
                      ? completed.handles - baseline.handles
                      : 0)
              << '\n';
    return 0;
}
