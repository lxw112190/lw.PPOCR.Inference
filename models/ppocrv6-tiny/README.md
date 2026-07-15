# PP-OCRv6 Tiny Experience Model

This lightweight package is included so the Windows Demo can run without
additional model setup. OpenCV DNN, DirectML, and OpenVINO select the `onnx`
artifacts. TensorRT selects the bundled `tensorrt` FP16 artifacts through the
same relative-path manifest.

The bundled engines are for local experience with the repository's tested
TensorRT 10 and CUDA 12 stack. TensorRT engines are not generally portable
across GPU architectures or TensorRT versions. Regenerate them from the ONNX
files on each deployment target before distributing a production package.
