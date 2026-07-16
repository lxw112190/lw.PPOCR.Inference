#define directml onnxruntime_backend
#define DirectMlOcrEngine OnnxRuntimeOcrEngine
#define LW_PPOCR_ORT_CUDA_RUNTIME 1
#define LW_PPOCR_ORT_BACKEND_ID LW_PPOCR_BACKEND_ONNXRUNTIME
#define LW_PPOCR_ORT_LABEL "ONNX Runtime"
#define LW_PPOCR_ORT_CAPABILITY_NAME "ONNX Runtime CPU / CUDA"
#define LW_PPOCR_ORT_RUNTIME_API_NAME "lw.PPOCR ONNX Runtime CPU / CUDA Runtime"
#include "../directml/runtime_entry.cpp"
