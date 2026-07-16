#include <lw/ppocr/ppocr_api.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

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

void LW_PPOCR_CALL LogCallback(
    lw_ppocr_log_level,
    const char* message_utf8,
    void*) {
    if (message_utf8 != nullptr) {
        std::cout << "Runtime log: " << message_utf8 << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return Fail("expected the Stub Runtime path");
    }

    lw_ppocr_version version{};
    version.struct_size = sizeof(version);
    if (lw_ppocr_get_version(&version) != LW_PPOCR_STATUS_OK ||
        version.api_version != LW_PPOCR_API_VERSION) {
        return Fail("version query failed");
    }

    lw_ppocr_config config{};
    config.struct_size = sizeof(config);
    config.api_version = LW_PPOCR_API_VERSION;
    config.backend = LW_PPOCR_BACKEND_OPENCV_DNN;
    config.device_id = 0;
    config.runtime_library_utf8 = argv[1];
    config.model_manifest_utf8 = "stub://model-manifest";
    config.enable_cls = 1;
    config.limit_side_len = 960;
    config.det_db_thresh = 0.3f;
    config.det_db_box_thresh = 0.6f;
    config.det_db_unclip_ratio = 1.6f;
    config.cls_threshold = 0.9f;
    config.cls_batch_size = 8;
    config.rec_batch_size = 1;
    config.rec_concurrency = 1;
    config.log_level = LW_PPOCR_LOG_INFO;
    config.log_callback = &LogCallback;

    lw_ppocr_handle engine = nullptr;
    const lw_ppocr_status create_status = lw_ppocr_create(&config, &engine);
    if (create_status != LW_PPOCR_STATUS_OK || engine == nullptr) {
        return Fail("create failed: " + LastError(nullptr));
    }

    lw_ppocr_capabilities capabilities{};
    capabilities.struct_size = sizeof(capabilities);
    if (lw_ppocr_get_capabilities(engine, &capabilities) != LW_PPOCR_STATUS_OK ||
        capabilities.supports_cpu != 1) {
        lw_ppocr_destroy(&engine);
        return Fail("capability query failed");
    }

    unsigned char pixels[12] = {};
    lw_ppocr_image image{};
    image.struct_size = sizeof(image);
    image.api_version = LW_PPOCR_API_VERSION;
    image.data = pixels;
    image.data_size = sizeof(pixels);
    image.width = 2;
    image.height = 2;
    image.stride = 6;
    image.pixel_format = LW_PPOCR_PIXEL_FORMAT_BGR24;

    lw_ppocr_result* result = nullptr;
    if (lw_ppocr_run(engine, &image, &result) != LW_PPOCR_STATUS_OK ||
        result == nullptr || result->region_count != 0) {
        const std::string error = LastError(engine);
        lw_ppocr_destroy(&engine);
        return Fail("structured run failed: " + error);
    }
    lw_ppocr_result_free(engine, result);

    char* json = nullptr;
    uint64_t json_length = 0;
    if (lw_ppocr_run_json(engine, &image, &json, &json_length) !=
            LW_PPOCR_STATUS_OK ||
        json == nullptr || json_length != 2 || std::strcmp(json, "[]") != 0) {
        const std::string error = LastError(engine);
        lw_ppocr_destroy(&engine);
        return Fail("JSON run failed: " + error);
    }
    lw_ppocr_string_free(engine, json);

    lw_ppocr_image crops[2] = {image, image};
    lw_ppocr_recognition_result* recognition = nullptr;
    if (lw_ppocr_recognize_batch(engine, crops, 2, &recognition) !=
            LW_PPOCR_STATUS_OK || recognition == nullptr ||
        recognition->item_count != 2 || recognition->items == nullptr ||
        recognition->items[0].source_index != 0 ||
        recognition->items[1].source_index != 1) {
        const std::string error = LastError(engine);
        lw_ppocr_destroy(&engine);
        return Fail("recognition-only batch failed: " + error);
    }
    lw_ppocr_recognition_result_free(engine, recognition);

    lw_ppocr_destroy(&engine);
    if (engine != nullptr) {
        return Fail("destroy did not clear the handle");
    }

    std::cout << version.product_name_utf8 << " " << version.version_utf8
              << " ABI smoke test passed\n";
    return 0;
}
