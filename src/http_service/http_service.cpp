#include "base64.h"
#include "image_decoder.h"

#include <httplib.h>
#include <json.hpp>
#include <lw/ppocr/ppocr_api.h>

#include <atomic>
#include <algorithm>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <chrono>
#include <iomanip>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <winsvc.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

struct ServiceConfig {
    std::string listen_host = "127.0.0.1";
    int port = 8787;
    std::string backend = "opencv";
    fs::path runtime_root;
    fs::path runtime_library;
    std::string model_manifest;
    fs::path web_root;
    int device_id = 0;
    bool enable_classifier = true;
    std::string backend_options_json;
    size_t max_request_bytes = 20u * 1024u * 1024u;
    uint64_t max_image_pixels = 40u * 1000u * 1000u;
    size_t worker_threads = 4;
    std::string api_key;
    std::string service_name = "lw.PPOCR.HttpService";
    std::string service_display_name = "lw.PPOCR HTTP OCR Service";
};

std::atomic<httplib::Server*> g_active_server{nullptr};
fs::path g_config_path;
#if defined(_WIN32)
SERVICE_STATUS_HANDLE g_service_status_handle = nullptr;
SERVICE_STATUS g_service_status{};
std::wstring g_service_name;

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        throw std::runtime_error("invalid UTF-8 text in configuration");
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), length);
    return result;
}
#endif

fs::path ExecutablePath() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        throw std::runtime_error("unable to determine executable path");
    }
    buffer.resize(length);
    return fs::path(buffer);
#else
    std::vector<char> buffer(PATH_MAX + 1u, '\0');
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), PATH_MAX);
    if (length <= 0 || length >= PATH_MAX) {
        throw std::runtime_error("unable to determine executable path");
    }
    return fs::u8path(std::string(buffer.data(), static_cast<size_t>(length)));
#endif
}

fs::path ResolvePath(const fs::path& base, const std::string& configured) {
    fs::path path = fs::u8path(configured);
    if (path.is_relative()) {
        path = base / path;
    }
    return fs::weakly_canonical(path);
}

ServiceConfig LoadConfig(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("unable to open configuration: " +
            path.u8string());
    }
    json document;
    input >> document;
    if (!document.is_object()) {
        throw std::runtime_error("HTTP service configuration must be an object");
    }

    const fs::path base = fs::absolute(path).parent_path();
    ServiceConfig config;
    config.listen_host = document.value("listen_host", config.listen_host);
    config.port = document.value("port", config.port);
    config.backend = document.value("backend", config.backend);
    config.device_id = document.value("device_id", config.device_id);
    config.enable_classifier =
        document.value("enable_classifier", config.enable_classifier);
    config.max_request_bytes =
        document.value("max_request_bytes", config.max_request_bytes);
    config.max_image_pixels =
        document.value("max_image_pixels", config.max_image_pixels);
    config.worker_threads =
        document.value("worker_threads", config.worker_threads);
    config.api_key = document.value("api_key", std::string{});
#if defined(_WIN32)
    constexpr const char* default_runtime_root = "runtimes/win-x64";
#else
    constexpr const char* default_runtime_root = "runtimes/linux-x64";
#endif
    config.runtime_root = ResolvePath(base,
        document.value("runtime_root", default_runtime_root));
    const std::string runtime_library =
        document.value("runtime_library", std::string{});
    if (!runtime_library.empty()) {
        config.runtime_library = ResolvePath(base, runtime_library);
    }
    const std::string model_manifest =
        document.value("model_manifest", "models/ppocrv6-tiny/model.json");
    config.model_manifest = model_manifest.find("://") != std::string::npos
        ? model_manifest
        : ResolvePath(base, model_manifest).u8string();
    config.web_root = ResolvePath(base,
        document.value("web_root", "www"));
    if (document.contains("backend_options")) {
        config.backend_options_json = document["backend_options"].dump();
    }
    config.service_name = document.value("service_name",
        "lw.PPOCR.HttpService." + config.backend);
    config.service_display_name = document.value(
        "service_display_name",
        "lw.PPOCR HTTP OCR Service (" + config.backend + ")");

    if (config.port < 1 || config.port > 65535) {
        throw std::runtime_error("port must be between 1 and 65535");
    }
    if (config.worker_threads < 1 || config.worker_threads > 64) {
        throw std::runtime_error("worker_threads must be between 1 and 64");
    }
    if (config.max_request_bytes < 1024 ||
        config.max_request_bytes > 256u * 1024u * 1024u) {
        throw std::runtime_error("max_request_bytes is outside the safe range");
    }
    if (config.max_image_pixels < 1 || config.max_image_pixels > 200000000u) {
        throw std::runtime_error("max_image_pixels is outside the safe range");
    }
    return config;
}

lw_ppocr_backend ParseBackend(const std::string& value) {
    if (value == "opencv" || value == "opencv-dnn") {
        return LW_PPOCR_BACKEND_OPENCV_DNN;
    }
    if (value == "directml" || value == "dml") {
        return LW_PPOCR_BACKEND_DIRECTML;
    }
    if (value == "openvino") {
        return LW_PPOCR_BACKEND_OPENVINO;
    }
    if (value == "tensorrt" || value == "trt") {
        return LW_PPOCR_BACKEND_TENSORRT;
    }
    if (value == "onnxruntime" || value == "ort") {
        return LW_PPOCR_BACKEND_ONNXRUNTIME;
    }
    throw std::runtime_error(
        "backend must be opencv, directml, openvino, tensorrt, or onnxruntime");
}

std::string LastError(lw_ppocr_handle handle) {
    const uint64_t required = lw_ppocr_get_last_error(handle, nullptr, 0);
    if (required == 0 || required > 1024u * 1024u) {
        return "unknown native OCR error";
    }
    std::vector<char> buffer(static_cast<size_t>(required));
    lw_ppocr_get_last_error(handle, buffer.data(), required);
    return std::string(buffer.data());
}

class OcrEngine {
public:
    struct Output {
        json result;
        json timing;
    };

    explicit OcrEngine(const ServiceConfig& settings) {
        runtime_root_ = settings.runtime_root.u8string();
        runtime_library_ = settings.runtime_library.empty()
            ? std::string{} : settings.runtime_library.u8string();
        model_manifest_ = settings.model_manifest;
        backend_options_ = settings.backend_options_json;
        if (backend_options_.empty() && settings.backend == "openvino") {
            backend_options_ = R"({"device":"CPU"})";
        }

        lw_ppocr_config config{};
        config.struct_size = sizeof(config);
        config.api_version = LW_PPOCR_API_VERSION;
        config.backend = ParseBackend(settings.backend);
        config.device_id = settings.device_id;
        config.runtime_root_utf8 = runtime_root_.c_str();
        config.runtime_library_utf8 = runtime_library_.empty()
            ? nullptr : runtime_library_.c_str();
        config.model_manifest_utf8 = model_manifest_.c_str();
        config.backend_options_json_utf8 = backend_options_.empty()
            ? nullptr : backend_options_.c_str();
        config.enable_cls = settings.enable_classifier ? 1 : 0;
        config.limit_side_len = 960;
        config.det_db_thresh = 0.3f;
        config.det_db_box_thresh = 0.6f;
        config.det_db_unclip_ratio = 1.6f;
        config.cls_threshold = 0.9f;
        config.cls_batch_size = 8;
        config.rec_batch_size = config.backend == LW_PPOCR_BACKEND_OPENVINO
            ? 1 : 4;
        config.rec_concurrency = config.backend == LW_PPOCR_BACKEND_OPENVINO
            ? 8 : 4;

        const lw_ppocr_status status = lw_ppocr_create(&config, &handle_);
        if (status != LW_PPOCR_STATUS_OK) {
            throw std::runtime_error("OCR initialization failed: " +
                LastError(handle_));
        }

        capabilities_.struct_size = sizeof(capabilities_);
        capabilities_.api_version = LW_PPOCR_API_VERSION;
        if (lw_ppocr_get_capabilities(handle_, &capabilities_) !=
            LW_PPOCR_STATUS_OK) {
            const std::string error = LastError(handle_);
            lw_ppocr_destroy(&handle_);
            throw std::runtime_error("capability query failed: " + error);
        }
    }

    ~OcrEngine() { lw_ppocr_destroy(&handle_); }
    OcrEngine(const OcrEngine&) = delete;
    OcrEngine& operator=(const OcrEngine&) = delete;

    std::string backend_name() const {
        return capabilities_.backend_name_utf8 == nullptr
            ? "unknown" : capabilities_.backend_name_utf8;
    }

    Output Run(const lw::ppocr::http::DecodedImage& decoded) const {
        lw_ppocr_image image{};
        image.struct_size = sizeof(image);
        image.api_version = LW_PPOCR_API_VERSION;
        image.data = decoded.pixels.data();
        image.data_size = decoded.pixels.size();
        image.width = static_cast<int32_t>(decoded.width);
        image.height = static_cast<int32_t>(decoded.height);
        image.stride = static_cast<int32_t>(decoded.stride);
        image.pixel_format = LW_PPOCR_PIXEL_FORMAT_BGR24;

        lw_ppocr_result* native = nullptr;
        const lw_ppocr_status status = lw_ppocr_run(handle_, &image, &native);
        if (status != LW_PPOCR_STATUS_OK) {
            throw std::runtime_error(LastError(handle_));
        }
        try {
            json regions = json::array();
            for (uint64_t index = 0; index < native->region_count; ++index) {
                const lw_ppocr_text_region& region = native->regions[index];
                regions.push_back({
                    {"text", region.text_utf8 == nullptr ? "" : region.text_utf8},
                    {"score", region.score},
                    {"cls_label", region.cls_label},
                    {"cls_score", region.cls_score},
                    {"x1", region.box.top_left.x},
                    {"y1", region.box.top_left.y},
                    {"x2", region.box.top_right.x},
                    {"y2", region.box.top_right.y},
                    {"x3", region.box.bottom_right.x},
                    {"y3", region.box.bottom_right.y},
                    {"x4", region.box.bottom_left.x},
                    {"y4", region.box.bottom_left.y}
                });
            }
            Output output;
            output.result = std::move(regions);
            output.timing = {
                {"detector", TimingJson(native->detector)},
                {"classifier", TimingJson(native->classifier)},
                {"recognizer", TimingJson(native->recognizer)},
                {"pipeline", TimingJson(native->pipeline)}
            };
            lw_ppocr_result_free(handle_, native);
            return output;
        } catch (...) {
            lw_ppocr_result_free(handle_, native);
            throw;
        }
    }

    Output RecognizeBatch(
        const std::vector<lw::ppocr::http::DecodedImage>& decoded) const {
        std::vector<lw_ppocr_image> images(decoded.size());
        for (size_t index = 0; index < decoded.size(); ++index) {
            images[index].struct_size = sizeof(lw_ppocr_image);
            images[index].api_version = LW_PPOCR_API_VERSION;
            images[index].data = decoded[index].pixels.data();
            images[index].data_size = decoded[index].pixels.size();
            images[index].width = static_cast<int32_t>(decoded[index].width);
            images[index].height = static_cast<int32_t>(decoded[index].height);
            images[index].stride = static_cast<int32_t>(decoded[index].stride);
            images[index].pixel_format = LW_PPOCR_PIXEL_FORMAT_BGR24;
        }

        lw_ppocr_recognition_result* native = nullptr;
        const lw_ppocr_status status = lw_ppocr_recognize_batch(handle_,
            images.data(), static_cast<uint64_t>(images.size()), &native);
        if (status != LW_PPOCR_STATUS_OK) {
            throw std::runtime_error(LastError(handle_));
        }
        try {
            json items = json::array();
            for (uint64_t index = 0; index < native->item_count; ++index) {
                const lw_ppocr_recognition& item = native->items[index];
                items.push_back({
                    {"source_index", item.source_index},
                    {"text", item.text_utf8 == nullptr ? "" : item.text_utf8},
                    {"score", item.score},
                    {"cls_label", item.cls_label},
                    {"cls_score", item.cls_score}
                });
            }
            Output output;
            output.result = std::move(items);
            output.timing = {
                {"classifier", TimingJson(native->classifier)},
                {"recognizer", TimingJson(native->recognizer)},
                {"pipeline", TimingJson(native->pipeline)}
            };
            lw_ppocr_recognition_result_free(handle_, native);
            return output;
        } catch (...) {
            lw_ppocr_recognition_result_free(handle_, native);
            throw;
        }
    }

private:
    static json TimingJson(const lw_ppocr_timing& timing) {
        return {
            {"preprocess_ms", timing.preprocess_ms},
            {"inference_ms", timing.inference_ms},
            {"postprocess_ms", timing.postprocess_ms},
            {"total_ms", timing.total_ms}
        };
    }

    std::string runtime_root_;
    std::string runtime_library_;
    std::string model_manifest_;
    std::string backend_options_;
    lw_ppocr_handle handle_ = nullptr;
    lw_ppocr_capabilities capabilities_{};
};

std::string ReadTextFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("unable to open web asset: " + path.u8string());
    }
    return std::string(std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void SetJson(httplib::Response& response, const json& value, int status = 200) {
    response.status = status;
    response.set_header("Cache-Control", "no-store");
    response.set_content(value.dump(), "application/json; charset=utf-8");
}

#if defined(_WIN32)
void AppendServiceLog(const std::string& message) {
    try {
        std::ofstream output(g_config_path.parent_path() / "http-service.log",
            std::ios::app);
        const auto now = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        output << std::put_time(&local, "%Y-%m-%d %H:%M:%S")
               << " " << message << '\n';
    } catch (...) {
    }
}
#endif

bool ConstantTimeEquals(const std::string& left, const std::string& right) {
    size_t difference = left.size() ^ right.size();
    const size_t maximum = (std::max)(left.size(), right.size());
    for (size_t index = 0; index < maximum; ++index) {
        const unsigned char left_value = index < left.size()
            ? static_cast<unsigned char>(left[index]) : 0;
        const unsigned char right_value = index < right.size()
            ? static_cast<unsigned char>(right[index]) : 0;
        difference |= left_value ^ right_value;
    }
    return difference == 0;
}

bool Authorized(const ServiceConfig& config,
                const httplib::Request& request,
                httplib::Response& response) {
    if (config.api_key.empty()) {
        return true;
    }
    const std::string supplied = request.get_header_value("X-API-Key");
    if (ConstantTimeEquals(supplied, config.api_key)) {
        return true;
    }
    SetJson(response, {{"ok", false}, {"error", "invalid API key"}}, 401);
    return false;
}

void PrintStartupInfo(const ServiceConfig& config, bool service_mode) {
    std::ostringstream startup;
    startup
        << "============================================================\n"
        << "lw.PPOCR.Inference HTTP Service v1.3.0\n"
        << "Author / 作者: 天天代码码天天\n"
        << "QQ: 819069052\n"
        << "============================================================\n"
        << "Startup parameters / 启动参数\n"
        << "  config_file: " << g_config_path.u8string() << '\n'
        << "  listen_host: " << config.listen_host << '\n'
        << "  port: " << config.port << '\n'
        << "  backend: " << config.backend << '\n'
        << "  runtime_root: " << config.runtime_root.u8string() << '\n'
        << "  runtime_library: "
        << (config.runtime_library.empty() ? "<auto>" :
            config.runtime_library.u8string()) << '\n'
        << "  model_manifest: " << config.model_manifest << '\n'
        << "  web_root: " << config.web_root.u8string() << '\n'
        << "  device_id: " << config.device_id << '\n'
        << "  enable_classifier: "
        << (config.enable_classifier ? "true" : "false") << '\n'
        << "  backend_options: "
        << (config.backend_options_json.empty() ? "<none>" :
            config.backend_options_json) << '\n'
        << "  worker_threads: " << config.worker_threads << '\n'
        << "  max_request_bytes: " << config.max_request_bytes << '\n'
        << "  max_image_pixels: " << config.max_image_pixels << '\n'
        << "  api_key: "
        << (config.api_key.empty() ? "disabled / 未启用" :
            "configured / 已配置（value hidden / 明文已隐藏）") << '\n'
        << "  service_name: " << config.service_name << '\n'
        << "  service_display_name: " << config.service_display_name << '\n'
        << "Request parameters / 请求参数\n"
        << "  POST /api/ocr: {\"image_base64\":\"...\"}\n"
        << "  POST /api/recognize: {\"image_base64\":\"...\"}\n"
        << "  POST /api/recognize batch: {\"images_base64\":[\"...\"]}\n"
        << "  authentication: X-API-Key header when api_key is configured\n"
        << "============================================================\n";
    std::cout << startup.str() << std::flush;
#if defined(_WIN32)
    if (service_mode) {
        AppendServiceLog(startup.str());
    }
#else
    (void)service_mode;
#endif
}

int RunHttpServer(const ServiceConfig& config, bool service_mode) {
    PrintStartupInfo(config, service_mode);
    OcrEngine engine(config);
    const std::string index_html = ReadTextFile(config.web_root / "index.html");

    httplib::Server server;
    server.set_payload_max_length(config.max_request_bytes);
    server.new_task_queue = [&config] {
        return new httplib::ThreadPool(config.worker_threads);
    };
    g_active_server.store(&server);

    server.Get("/", [&index_html](const httplib::Request&,
                                  httplib::Response& response) {
        response.set_content(index_html, "text/html; charset=utf-8");
    });
    server.Get("/health", [&config, &engine](const httplib::Request&,
                                             httplib::Response& response) {
        SetJson(response, {
            {"ok", true},
            {"status", "ready"},
            {"backend", config.backend},
            {"backend_name", engine.backend_name()},
            {"api_key_required", !config.api_key.empty()},
            {"max_request_bytes", config.max_request_bytes},
            {"max_image_pixels", config.max_image_pixels}
        });
    });
    server.Post("/api/ocr", [&config, &engine](const httplib::Request& request,
                                                httplib::Response& response) {
        if (!Authorized(config, request, response)) {
            return;
        }
        try {
            const auto request_start = std::chrono::steady_clock::now();
            const json body = json::parse(request.body);
            if (!body.is_object() || !body.contains("image_base64") ||
                !body["image_base64"].is_string()) {
                SetJson(response, {{"ok", false},
                    {"error", "image_base64 string is required"}}, 400);
                return;
            }

            std::vector<uint8_t> encoded;
            std::string error;
            if (!lw::ppocr::http::DecodeBase64(
                    body["image_base64"].get_ref<const std::string&>(),
                    config.max_request_bytes, encoded, error)) {
                SetJson(response, {{"ok", false}, {"error", error}}, 400);
                return;
            }

            lw::ppocr::http::DecodedImage decoded;
            if (!lw::ppocr::http::DecodeImage(encoded,
                    config.max_image_pixels, decoded, error)) {
                SetJson(response, {{"ok", false}, {"error", error}}, 400);
                return;
            }

            const auto decode_end = std::chrono::steady_clock::now();
            OcrEngine::Output output = engine.Run(decoded);
            const auto inference_end = std::chrono::steady_clock::now();
            output.timing["decode_ms"] =
                std::chrono::duration<double, std::milli>(
                    decode_end - request_start).count();
            output.timing["server_total_ms"] =
                std::chrono::duration<double, std::milli>(
                    inference_end - request_start).count();

            SetJson(response, {
                {"ok", true},
                {"backend", config.backend},
                {"image", {
                    {"width", decoded.width},
                    {"height", decoded.height}
                }},
                {"result", std::move(output.result)},
                {"timing", std::move(output.timing)}
            });
        } catch (const json::exception& exception) {
            SetJson(response, {{"ok", false},
                {"error", std::string("invalid JSON request: ") +
                    exception.what()}}, 400);
        } catch (const std::exception& exception) {
            SetJson(response, {{"ok", false}, {"error", exception.what()}}, 500);
        }
    });
    server.Post("/api/recognize", [&config, &engine](
        const httplib::Request& request, httplib::Response& response) {
        if (!Authorized(config, request, response)) {
            return;
        }
        try {
            const auto request_start = std::chrono::steady_clock::now();
            const json body = json::parse(request.body);
            std::vector<std::string> encoded_images;
            if (body.is_object() && body.contains("image_base64") &&
                body["image_base64"].is_string()) {
                encoded_images.push_back(body["image_base64"].get<std::string>());
            } else if (body.is_object() && body.contains("images_base64") &&
                body["images_base64"].is_array()) {
                if (body["images_base64"].empty() ||
                    body["images_base64"].size() > 256) {
                    SetJson(response, {{"ok", false},
                        {"error", "images_base64 must contain 1 to 256 images"}}, 400);
                    return;
                }
                for (const json& value : body["images_base64"]) {
                    if (!value.is_string()) {
                        SetJson(response, {{"ok", false},
                            {"error", "every images_base64 item must be a string"}}, 400);
                        return;
                    }
                    encoded_images.push_back(value.get<std::string>());
                }
            } else {
                SetJson(response, {{"ok", false}, {"error",
                    "image_base64 or images_base64 is required"}}, 400);
                return;
            }

            std::vector<lw::ppocr::http::DecodedImage> decoded_images;
            decoded_images.reserve(encoded_images.size());
            std::string error;
            for (size_t index = 0; index < encoded_images.size(); ++index) {
                std::vector<uint8_t> encoded;
                if (!lw::ppocr::http::DecodeBase64(encoded_images[index],
                        config.max_request_bytes, encoded, error)) {
                    SetJson(response, {{"ok", false}, {"error",
                        "image " + std::to_string(index) + ": " + error}}, 400);
                    return;
                }
                lw::ppocr::http::DecodedImage decoded;
                if (!lw::ppocr::http::DecodeImage(encoded,
                        config.max_image_pixels, decoded, error)) {
                    SetJson(response, {{"ok", false}, {"error",
                        "image " + std::to_string(index) + ": " + error}}, 400);
                    return;
                }
                decoded_images.push_back(std::move(decoded));
            }

            const auto decode_end = std::chrono::steady_clock::now();
            OcrEngine::Output output = engine.RecognizeBatch(decoded_images);
            const auto inference_end = std::chrono::steady_clock::now();
            output.timing["decode_ms"] =
                std::chrono::duration<double, std::milli>(
                    decode_end - request_start).count();
            output.timing["server_total_ms"] =
                std::chrono::duration<double, std::milli>(
                    inference_end - request_start).count();
            SetJson(response, {
                {"ok", true},
                {"backend", config.backend},
                {"image_count", decoded_images.size()},
                {"result", std::move(output.result)},
                {"timing", std::move(output.timing)}
            });
        } catch (const json::exception& exception) {
            SetJson(response, {{"ok", false},
                {"error", std::string("invalid JSON request: ") +
                    exception.what()}}, 400);
        } catch (const std::exception& exception) {
            SetJson(response, {{"ok", false}, {"error", exception.what()}}, 500);
        }
    });
    server.set_error_handler([](const httplib::Request&,
                                httplib::Response& response) {
        if (response.status == 413) {
            SetJson(response, {{"ok", false},
                {"error", "request exceeds max_request_bytes"}}, 413);
        }
    });
    server.set_start_handler([&config, &engine, service_mode] {
        std::cout << "lw.PPOCR HTTP service ready: http://"
                  << config.listen_host << ':' << config.port
                  << " (" << engine.backend_name() << ")\n";
#if defined(_WIN32)
        if (service_mode && g_service_status_handle != nullptr) {
            g_service_status.dwCurrentState = SERVICE_RUNNING;
            g_service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP |
                SERVICE_ACCEPT_SHUTDOWN;
            g_service_status.dwWaitHint = 0;
            SetServiceStatus(g_service_status_handle, &g_service_status);
        }
#else
        (void)service_mode;
#endif
    });

    const bool listened = server.listen(config.listen_host, config.port);
    g_active_server.store(nullptr);
    if (!listened) {
        throw std::runtime_error("unable to listen on configured host and port");
    }
    return 0;
}

#if defined(_WIN32)
void ReportServiceStopped(DWORD exit_code) {
    if (g_service_status_handle == nullptr) {
        return;
    }
    g_service_status.dwCurrentState = SERVICE_STOPPED;
    g_service_status.dwControlsAccepted = 0;
    g_service_status.dwWin32ExitCode = exit_code;
    SetServiceStatus(g_service_status_handle, &g_service_status);
}

DWORD WINAPI ServiceControlHandler(DWORD control, DWORD, void*, void*) {
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        g_service_status.dwCurrentState = SERVICE_STOP_PENDING;
        g_service_status.dwControlsAccepted = 0;
        SetServiceStatus(g_service_status_handle, &g_service_status);
        if (httplib::Server* server = g_active_server.load()) {
            server->stop();
        }
    }
    return NO_ERROR;
}

void WINAPI ServiceMain(DWORD, LPWSTR*) {
    g_service_status_handle = RegisterServiceCtrlHandlerExW(
        g_service_name.c_str(), ServiceControlHandler, nullptr);
    if (g_service_status_handle == nullptr) {
        return;
    }
    g_service_status = {};
    g_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_service_status.dwCurrentState = SERVICE_START_PENDING;
    g_service_status.dwWin32ExitCode = NO_ERROR;
    g_service_status.dwWaitHint = 120000;
    SetServiceStatus(g_service_status_handle, &g_service_status);
    try {
        RunHttpServer(LoadConfig(g_config_path), true);
        ReportServiceStopped(NO_ERROR);
    } catch (const std::exception& exception) {
        AppendServiceLog(std::string("startup failed: ") + exception.what());
        ReportServiceStopped(ERROR_SERVICE_SPECIFIC_ERROR);
    } catch (...) {
        AppendServiceLog("startup failed: unknown error");
        ReportServiceStopped(ERROR_SERVICE_SPECIFIC_ERROR);
    }
}

BOOL WINAPI ConsoleControlHandler(DWORD control) {
    if (control == CTRL_C_EVENT || control == CTRL_BREAK_EVENT ||
        control == CTRL_CLOSE_EVENT || control == CTRL_SHUTDOWN_EVENT) {
        if (httplib::Server* server = g_active_server.load()) {
            server->stop();
        }
        return TRUE;
    }
    return FALSE;
}

class ServiceHandle {
public:
    explicit ServiceHandle(SC_HANDLE value = nullptr) : value_(value) {}
    ~ServiceHandle() { if (value_ != nullptr) CloseServiceHandle(value_); }
    SC_HANDLE get() const noexcept { return value_; }
    explicit operator bool() const noexcept { return value_ != nullptr; }
private:
    SC_HANDLE value_;
};

std::runtime_error WindowsError(const char* operation) {
    return std::runtime_error(std::string(operation) +
        " failed (Win32=" + std::to_string(GetLastError()) + ")");
}

void InstallService(const ServiceConfig& config, const fs::path& config_path) {
    ServiceHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
    if (!manager) {
        throw WindowsError("OpenSCManager");
    }
    const std::wstring command = L"\"" + ExecutablePath().wstring() +
        L"\" --service --config \"" + fs::absolute(config_path).wstring() + L"\"";
    const std::wstring service_name = Utf8ToWide(config.service_name);
    const std::wstring service_display_name =
        Utf8ToWide(config.service_display_name);
    ServiceHandle service(CreateServiceW(manager.get(), service_name.c_str(),
        service_display_name.c_str(), SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        command.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr));
    if (!service) {
        if (GetLastError() == ERROR_SERVICE_EXISTS) {
            throw std::runtime_error("service already exists; uninstall it first");
        }
        throw WindowsError("CreateService");
    }
    SERVICE_DESCRIPTIONW description{};
    std::wstring text = L"Local HTTP API and test page for lw.PPOCR.Inference (" +
        Utf8ToWide(config.backend) + L")";
    description.lpDescription = text.data();
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DESCRIPTION, &description);
    SERVICE_DELAYED_AUTO_START_INFO delayed{TRUE};
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
        &delayed);
    if (!StartServiceW(service.get(), 0, nullptr) &&
        GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
        throw WindowsError("StartService");
    }
    std::wcout << L"Installed and started service: " << service_name << L'\n';
}

void StopServiceIfRunning(SC_HANDLE service) {
    SERVICE_STATUS_PROCESS status{};
    DWORD bytes_needed = 0;
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
            reinterpret_cast<BYTE*>(&status), sizeof(status), &bytes_needed)) {
        throw WindowsError("QueryServiceStatusEx");
    }
    if (status.dwCurrentState == SERVICE_STOPPED) {
        return;
    }
    SERVICE_STATUS temporary{};
    if (!ControlService(service, SERVICE_CONTROL_STOP, &temporary) &&
        GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
        throw WindowsError("ControlService");
    }
    for (int attempt = 0; attempt < 120; ++attempt) {
        Sleep(250);
        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                reinterpret_cast<BYTE*>(&status), sizeof(status), &bytes_needed)) {
            throw WindowsError("QueryServiceStatusEx");
        }
        if (status.dwCurrentState == SERVICE_STOPPED) {
            return;
        }
    }
    throw std::runtime_error("timed out while stopping the service");
}

void UninstallService(const ServiceConfig& config) {
    ServiceHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!manager) {
        throw WindowsError("OpenSCManager");
    }
    const std::wstring service_name = Utf8ToWide(config.service_name);
    ServiceHandle service(OpenServiceW(manager.get(), service_name.c_str(),
        SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE));
    if (!service) {
        if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
            std::wcout << L"Service is not installed: " << service_name << L'\n';
            return;
        }
        throw WindowsError("OpenService");
    }
    StopServiceIfRunning(service.get());
    if (!DeleteService(service.get())) {
        throw WindowsError("DeleteService");
    }
    std::wcout << L"Uninstalled service: " << service_name << L'\n';
}
#else
void SignalHandler(int) {
    if (httplib::Server* server = g_active_server.load()) {
        server->stop();
    }
}
#endif

void PrintUsage() {
#if defined(_WIN32)
    std::cout
        << "lw.PPOCR.HttpService [--console|--service|--install|--uninstall] "
           "[--config <path>]\n"
        << "  --console    Run in the foreground (default)\n"
        << "  --service    Run under Windows Service Control Manager\n"
        << "  --install    Install and start an automatic Windows service (admin)\n"
        << "  --uninstall  Stop and remove the Windows service (admin)\n";
#else
    std::cout
        << "lw-ppocr-http-service [--console] [--config <path>]\n"
        << "  --console    Run in the foreground (default)\n"
        << "Linux service installation is provided by install-systemd.sh.\n";
#endif
}

}  // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    try {
        std::wstring mode = L"--console";
        g_config_path = ExecutablePath().parent_path() / L"http-service.json";
        for (int index = 1; index < argc; ++index) {
            const std::wstring argument = argv[index];
            if (argument == L"--config" && index + 1 < argc) {
                g_config_path = fs::absolute(argv[++index]);
            } else if (argument == L"--console" || argument == L"--service" ||
                       argument == L"--install" || argument == L"--uninstall") {
                mode = argument;
            } else if (argument == L"--help" || argument == L"-h") {
                PrintUsage();
                return 0;
            } else {
                throw std::runtime_error("unknown or incomplete command-line option");
            }
        }

        const ServiceConfig config = LoadConfig(g_config_path);
        g_service_name = Utf8ToWide(config.service_name);
        if (mode == L"--install") {
            InstallService(config, g_config_path);
            return 0;
        }
        if (mode == L"--uninstall") {
            UninstallService(config);
            return 0;
        }
        if (mode == L"--service") {
            SERVICE_TABLE_ENTRYW table[] = {
                {const_cast<LPWSTR>(g_service_name.c_str()), ServiceMain},
                {nullptr, nullptr}
            };
            if (!StartServiceCtrlDispatcherW(table)) {
                throw WindowsError("StartServiceCtrlDispatcher");
            }
            return 0;
        }

        SetConsoleCtrlHandler(ConsoleControlHandler, TRUE);
        return RunHttpServer(config, false);
    } catch (const std::exception& exception) {
        std::cerr << "lw.PPOCR HTTP service: " << exception.what() << '\n';
        return 1;
    }
}
#else
int main(int argc, char** argv) {
    try {
        g_config_path = ExecutablePath().parent_path() / "http-service.json";
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--config" && index + 1 < argc) {
                g_config_path = fs::absolute(fs::u8path(argv[++index]));
            } else if (argument == "--console") {
                continue;
            } else if (argument == "--help" || argument == "-h") {
                PrintUsage();
                return 0;
            } else if (argument == "--service" || argument == "--install" ||
                       argument == "--uninstall") {
                throw std::runtime_error(
                    "Windows service options are unavailable on Linux; use the supplied systemd scripts");
            } else {
                throw std::runtime_error("unknown or incomplete command-line option");
            }
        }

        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);
        return RunHttpServer(LoadConfig(g_config_path), false);
    } catch (const std::exception& exception) {
        std::cerr << "lw.PPOCR HTTP service: " << exception.what() << '\n';
        return 1;
    }
}
#endif
