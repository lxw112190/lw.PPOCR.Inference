#include <lw/ppocr/ppocr_api.h>

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kBoxTolerance = 2.0;
constexpr double kScoreTolerance = 0.02;
constexpr int kGoldenSchemaVersion = 1;

int Fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

std::string LastError(lw_ppocr_handle handle) {
    const uint64_t required = lw_ppocr_get_last_error(handle, nullptr, 0);
    std::vector<char> buffer(static_cast<size_t>(required > 0 ? required : 1));
    lw_ppocr_get_last_error(handle, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

std::string JsonPath(const std::string& path) {
    std::string escaped;
    escaped.reserve(path.size());
    for (const char character : path) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(character);
    }
    return escaped;
}

void WriteManifest(
    const std::string& manifest_path,
    const std::string& detector,
    const std::string& recognizer,
    const std::string& dictionary,
    const std::string& classifier) {
    std::ofstream output(manifest_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot create golden test manifest");
    }
    output
        << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"name\": \"PP-OCR golden test\",\n"
        << "  \"family\": \"PP-OCR\",\n"
        << "  \"language\": \"ch\",\n"
        << "  \"dictionary\": {\"path\": \"" << JsonPath(dictionary) << "\"},\n"
        << "  \"stages\": {\n"
        << "    \"detector\": {\"input_shape\": [-1, 3, -1, -1], "
        << "\"artifacts\": {\"onnx\": {\"path\": \"" << JsonPath(detector)
        << "\", \"format\": \"onnx\", \"precision\": \"fp32\"}}},\n"
        << "    \"recognizer\": {\"input_shape\": [1, 3, 48, 320], "
        << "\"artifacts\": {\"onnx\": {\"path\": \"" << JsonPath(recognizer)
        << "\", \"format\": \"onnx\", \"precision\": \"fp32\"}}}";
    if (!classifier.empty()) {
        output
            << ",\n    \"classifier\": {\"input_shape\": [1, 3, 80, 160], "
            << "\"artifacts\": {\"onnx\": {\"path\": \"" << JsonPath(classifier)
            << "\", \"format\": \"onnx\", \"precision\": \"fp32\"}}}";
    }
    output
        << "\n"
        << "  }\n"
        << "}\n";
}

void WriteGolden(
    const std::string& golden_path,
    const std::string& model_manifest,
    const std::string& image_path,
    const lw_ppocr_result& result) {
    std::ofstream output(golden_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot write golden file: " + golden_path);
    }
    output << std::fixed << std::setprecision(6);
    output << "{\n"
           << "  \"schema_version\": " << kGoldenSchemaVersion << ",\n"
           << "  \"source\": {\n"
           << "    \"model\": \"" << JsonPath(model_manifest) << "\",\n"
           << "    \"image\": \"" << JsonPath(image_path) << "\"\n"
           << "  },\n"
           << "  \"expected\": {\n"
           << "    \"region_count\": " << result.region_count << ",\n"
           << "    \"regions\": [\n";
    for (uint64_t index = 0; index < result.region_count; ++index) {
        const lw_ppocr_text_region& region = result.regions[index];
        output << "      {\n"
               << "        \"text\": \"" << JsonPath(region.text_utf8 != nullptr
                   ? std::string(region.text_utf8) : std::string()) << "\",\n"
               << "        \"score\": " << region.score << ",\n"
               << "        \"cls_label\": " << region.cls_label << ",\n"
               << "        \"cls_score\": " << region.cls_score << ",\n"
               << "        \"box\": [\n"
               << "          [" << region.box.top_left.x << ", "
                                 << region.box.top_left.y << "],\n"
               << "          [" << region.box.top_right.x << ", "
                                 << region.box.top_right.y << "],\n"
               << "          [" << region.box.bottom_right.x << ", "
                                 << region.box.bottom_right.y << "],\n"
               << "          [" << region.box.bottom_left.x << ", "
                                 << region.box.bottom_left.y << "]\n"
               << "        ]\n"
               << "      }";
        if (index + 1 < result.region_count) {
            output << ',';
        }
        output << '\n';
    }
    output << "    ]\n"
           << "  }\n"
           << "}\n";
}

struct GoldenRegion {
    std::string text;
    float score = 0.0f;
    int cls_label = 0;
    float cls_score = 0.0f;
    float box[4][2] = {};
};

struct GoldenData {
    int region_count = 0;
    std::vector<GoldenRegion> regions;
};

GoldenData ReadGolden(const std::string& golden_path) {
    cv::FileStorage storage(golden_path,
        cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!storage.isOpened()) {
        throw std::runtime_error("failed to open golden file: " + golden_path);
    }

    int schema_version = 0;
    storage["schema_version"] >> schema_version;
    if (schema_version != kGoldenSchemaVersion) {
        throw std::runtime_error("golden file schema_version is not " +
            std::to_string(kGoldenSchemaVersion));
    }

    GoldenData golden;
    storage["expected"]["region_count"] >> golden.region_count;
    if (golden.region_count <= 0) {
        throw std::runtime_error("golden region_count must be positive");
    }

    const cv::FileNode regions_node = storage["expected"]["regions"];
    if (regions_node.empty() || !regions_node.isSeq()) {
        throw std::runtime_error("golden regions array is missing or invalid");
    }

    for (const cv::FileNode& region_node : regions_node) {
        GoldenRegion golden_region;
        region_node["text"] >> golden_region.text;
        region_node["score"] >> golden_region.score;
        region_node["cls_label"] >> golden_region.cls_label;
        region_node["cls_score"] >> golden_region.cls_score;

        const cv::FileNode box_node = region_node["box"];
        if (box_node.empty() || !box_node.isSeq() || box_node.size() != 4) {
            throw std::runtime_error("golden region box must have 4 points");
        }
        for (int point = 0; point < 4; ++point) {
            const cv::FileNode coord_node = box_node[point];
            if (!coord_node.isSeq() || coord_node.size() != 2) {
                throw std::runtime_error("golden box point must have 2 coordinates");
            }
            golden_region.box[point][0] = static_cast<float>(
                static_cast<double>(coord_node[0]));
            golden_region.box[point][1] = static_cast<float>(
                static_cast<double>(coord_node[1]));
        }
        golden.regions.push_back(std::move(golden_region));
    }
    return golden;
}

bool CompareRegions(
    const lw_ppocr_text_region& actual,
    const GoldenRegion& expected,
    uint64_t index,
    std::string& error) {
    std::ostringstream message;

    const std::string actual_text = actual.text_utf8 != nullptr
        ? std::string(actual.text_utf8) : std::string();
    if (actual_text != expected.text) {
        message << "region[" << index << "] text mismatch: expected \""
                << expected.text << "\", got \"" << actual_text << "\"";
        error = message.str();
        return false;
    }

    if (std::abs(actual.score - expected.score) > kScoreTolerance) {
        message << "region[" << index << "] score mismatch: expected "
                << expected.score << ", got " << actual.score
                << " (tolerance=" << kScoreTolerance << ")";
        error = message.str();
        return false;
    }

    if (actual.cls_label != expected.cls_label) {
        message << "region[" << index << "] cls_label mismatch: expected "
                << expected.cls_label << ", got " << actual.cls_label;
        error = message.str();
        return false;
    }

    if (std::abs(actual.cls_score - expected.cls_score) > kScoreTolerance) {
        message << "region[" << index << "] cls_score mismatch: expected "
                << expected.cls_score << ", got " << actual.cls_score
                << " (tolerance=" << kScoreTolerance << ")";
        error = message.str();
        return false;
    }

    const lw_ppocr_point* actual_points[] = {
        &actual.box.top_left, &actual.box.top_right,
        &actual.box.bottom_right, &actual.box.bottom_left
    };
    for (int point = 0; point < 4; ++point) {
        const double dx = std::abs(actual_points[point]->x -
            static_cast<double>(expected.box[point][0]));
        const double dy = std::abs(actual_points[point]->y -
            static_cast<double>(expected.box[point][1]));
        if (dx > kBoxTolerance || dy > kBoxTolerance) {
            message << "region[" << index << "] box[" << point
                    << "] mismatch: expected (" << expected.box[point][0]
                    << ", " << expected.box[point][1] << "), got ("
                    << actual_points[point]->x << ", "
                    << actual_points[point]->y
                    << ") (tolerance=" << kBoxTolerance << " px)";
            error = message.str();
            return false;
        }
    }
    return true;
}

bool Verify(const lw_ppocr_result& result, const GoldenData& golden) {
    if (static_cast<int>(result.region_count) != golden.region_count) {
        std::ostringstream message;
        message << "region_count mismatch: expected " << golden.region_count
                << ", got " << result.region_count;
        throw std::runtime_error(message.str());
    }

    for (int index = 0; index < golden.region_count; ++index) {
        std::string compare_error;
        if (!CompareRegions(result.regions[index], golden.regions[index],
                static_cast<uint64_t>(index), compare_error)) {
            throw std::runtime_error(compare_error);
        }
    }
    return true;
}

void LW_PPOCR_CALL LogCallback(
    lw_ppocr_log_level,
    const char* message_utf8,
    void*) {
    if (message_utf8 != nullptr) {
        std::cout << "[runtime] " << message_utf8 << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 8) {
        return Fail(
            "usage: golden_test <runtime.dll> <det.onnx> <rec.onnx> "
            "<dict.txt> <image.jpg> <cls.onnx> <golden.json>");
    }

    try {
        const std::string manifest_path =
            (std::filesystem::temp_directory_path() /
             "lw_ppocr_golden_manifest.json").string();
        const std::string classifier = argv[6];
        const bool has_classifier = !classifier.empty() &&
            classifier != "none" && classifier != "-";

        WriteManifest(manifest_path, argv[2], argv[3], argv[4],
            has_classifier ? classifier : std::string());

        const cv::Mat source = cv::imread(argv[5], cv::IMREAD_COLOR);
        if (source.empty()) {
            std::filesystem::remove(manifest_path);
            return Fail("failed to load test image: " + std::string(argv[5]));
        }

        lw_ppocr_config config{};
        config.struct_size = sizeof(config);
        config.api_version = LW_PPOCR_API_VERSION;
        config.backend = LW_PPOCR_BACKEND_OPENCV_DNN;
        config.device_id = 0;
        config.runtime_library_utf8 = argv[1];
        config.model_manifest_utf8 = manifest_path.c_str();
        config.enable_cls = has_classifier ? 1 : 0;
        config.limit_side_len = 960;
        config.det_db_thresh = 0.3f;
        config.det_db_box_thresh = 0.6f;
        config.det_db_unclip_ratio = 1.6f;
        config.det_use_dilation = 0;
        config.cls_threshold = 0.9f;
        config.cls_batch_size = 8;
        config.rec_batch_size = 1;
        config.rec_concurrency = 2;
        config.log_level = LW_PPOCR_LOG_INFO;
        config.log_callback = &LogCallback;

        lw_ppocr_handle engine = nullptr;
        const lw_ppocr_status create_status =
            lw_ppocr_create(&config, &engine);
        if (create_status != LW_PPOCR_STATUS_OK || engine == nullptr) {
            const std::string error = LastError(nullptr);
            std::filesystem::remove(manifest_path);
            return Fail("create failed: " + error);
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

        const char* record_env = std::getenv("LW_PPOCR_RECORD_GOLDEN");
        const bool record_mode = record_env != nullptr &&
            (std::string(record_env) == "1" || std::string(record_env) == "true");

        if (record_mode) {
            lw_ppocr_result* result = nullptr;
            const lw_ppocr_status run_status =
                lw_ppocr_run(engine, &image, &result);
            if (run_status != LW_PPOCR_STATUS_OK || result == nullptr) {
                const std::string error = LastError(engine);
                lw_ppocr_destroy(&engine);
                std::filesystem::remove(manifest_path);
                return Fail("RECORD run failed: " + error);
            }
            if (result->region_count == 0) {
                lw_ppocr_result_free(engine, result);
                lw_ppocr_destroy(&engine);
                std::filesystem::remove(manifest_path);
                return Fail("RECORD returned no text regions");
            }

            WriteGolden(argv[7], manifest_path, argv[5], *result);
            lw_ppocr_result_free(engine, result);
            lw_ppocr_destroy(&engine);
            std::filesystem::remove(manifest_path);
            std::cout << "Golden data recorded to " << argv[7]
                      << " (" << result->region_count << " regions)\n";
            return 0;
        }

        const GoldenData golden = ReadGolden(argv[7]);
        std::cout << "Golden data loaded: " << golden.region_count
                  << " regions\n";

        lw_ppocr_result* result = nullptr;
        const lw_ppocr_status run_status =
            lw_ppocr_run(engine, &image, &result);
        if (run_status != LW_PPOCR_STATUS_OK || result == nullptr) {
            const std::string error = LastError(engine);
            lw_ppocr_result_free(engine, result);
            lw_ppocr_destroy(&engine);
            std::filesystem::remove(manifest_path);
            return Fail("OCR inference failed: " + error);
        }

        Verify(*result, golden);

        std::cout << "Golden test passed: " << result->region_count
                  << " regions, all text/boxes/scores match\n";
        for (uint64_t index = 0; index < result->region_count; ++index) {
            const lw_ppocr_text_region& region = result->regions[index];
            std::cout << "  [" << index << "] "
                      << (region.text_utf8 != nullptr
                              ? region.text_utf8 : "")
                      << " (score=" << std::fixed << std::setprecision(4)
                      << region.score << ")\n";
        }

        lw_ppocr_result_free(engine, result);
        lw_ppocr_destroy(&engine);
        std::filesystem::remove(manifest_path);
        return 0;
    } catch (const cv::Exception& exception) {
        return Fail(std::string("OpenCV: ") + exception.what());
    } catch (const std::exception& exception) {
        return Fail(exception.what());
    }
}
