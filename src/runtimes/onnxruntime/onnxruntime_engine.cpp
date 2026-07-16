// The detector/classifier/recognizer pipeline is identical to the proven
// DirectML implementation. These token aliases compile that implementation
// with the portable ONNX Runtime CUDA/CPU session policy. Keep the shared
// implementation in one place so DirectML and Linux produce identical OCR.
#define directml onnxruntime_backend
#define DirectMlOcrEngine OnnxRuntimeOcrEngine
#define LW_PPOCR_ORT_CUDA_RUNTIME 1
#include "../directml/directml_engine.cpp"
