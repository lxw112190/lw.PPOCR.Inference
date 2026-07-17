#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"
PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
OPENCV_VERSION="${OPENCV_VERSION:-5.0.0}"
OPENCV_ROOT="${PROJECT_ROOT}/.ci/opencv-${OPENCV_VERSION}-openeuler-arm64"
BUILD_DIR="${PROJECT_ROOT}/build/ci-openeuler-arm64"
PACKAGE_VERSION="${PACKAGE_VERSION:-1.4.0-preview.1}"

if [[ "$(uname -m)" != "aarch64" ]]; then
  echo "This script must run natively on AArch64, got: $(uname -m)" >&2
  exit 1
fi

case "${MODE}" in
  opencv)
    if find "${OPENCV_ROOT}" -name OpenCVConfig.cmake -print -quit \
        2>/dev/null | grep -q .; then
      echo "Using cached OpenCV ${OPENCV_VERSION}: ${OPENCV_ROOT}"
      exit 0
    fi

    SOURCE_DIR="/tmp/lw-ppocr-opencv-source"
    OPENCV_BUILD_DIR="/tmp/lw-ppocr-opencv-build"
    ARCHIVE="/tmp/opencv-${OPENCV_VERSION}.tar.gz"
    rm -rf "${SOURCE_DIR}" "${OPENCV_BUILD_DIR}" "${OPENCV_ROOT}"
    mkdir -p "${SOURCE_DIR}" "${OPENCV_BUILD_DIR}" "${OPENCV_ROOT}"
    curl --fail --location --retry 5 --retry-delay 5 \
      --retry-connrefused \
      "https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VERSION}.tar.gz" \
      --output "${ARCHIVE}"
    tar -xzf "${ARCHIVE}" --strip-components=1 -C "${SOURCE_DIR}"

    cmake -S "${SOURCE_DIR}" -B "${OPENCV_BUILD_DIR}" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS_RELEASE='-O3 -DNDEBUG -march=armv8-a' \
      -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG -march=armv8-a' \
      -DCMAKE_INSTALL_PREFIX="${OPENCV_ROOT}" \
      -DCMAKE_INSTALL_RPATH='$ORIGIN' \
      -DBUILD_LIST=core,imgproc,imgcodecs,dnn \
      -DBUILD_SHARED_LIBS=ON \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_JAVA=OFF \
      -DBUILD_opencv_apps=OFF \
      -DBUILD_PERF_TESTS=OFF \
      -DBUILD_TESTS=OFF \
      -DBUILD_PROTOBUF=ON \
      -DWITH_EIGEN=OFF \
      -DWITH_FFMPEG=OFF \
      -DWITH_GDAL=OFF \
      -DWITH_GSTREAMER=OFF \
      -DWITH_GTK=OFF \
      -DWITH_IPP=OFF \
      -DWITH_ITT=OFF \
      -DWITH_JASPER=OFF \
      -DWITH_LAPACK=OFF \
      -DWITH_OPENCL=OFF \
      -DWITH_OPENEXR=OFF \
      -DWITH_QT=OFF \
      -DWITH_TBB=OFF \
      -DWITH_WEBP=OFF
    cmake --build "${OPENCV_BUILD_DIR}" --parallel 2
    cmake --install "${OPENCV_BUILD_DIR}"
    find "${OPENCV_ROOT}" -name OpenCVConfig.cmake -print -quit | grep -q .
    rm -rf "${SOURCE_DIR}" "${OPENCV_BUILD_DIR}" "${ARCHIVE}"
    ;;

  project)
    cat /etc/os-release
    getconf GNU_LIBC_VERSION
    test "$(uname -m)" = "aarch64"
    python3 "${PROJECT_ROOT}/scripts/validate-model-schema.py"

    OPENCV_CONFIG="$(find "${OPENCV_ROOT}" \
      -name OpenCVConfig.cmake -print -quit)"
    test -n "${OPENCV_CONFIG}"
    OPENCV_DIR="$(dirname -- "${OPENCV_CONFIG}")"
    OPENCV_LIB="${OPENCV_ROOT}/lib"

    cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS_RELEASE='-O3 -DNDEBUG -march=armv8-a' \
      -DCMAKE_CXX_FLAGS_RELEASE='-O3 -DNDEBUG -march=armv8-a' \
      -DOpenCV_DIR="${OPENCV_DIR}" \
      -DLW_PPOCR_BUILD_OPENCV_RUNTIME=ON \
      -DLW_PPOCR_BUILD_HTTP_SERVICE=ON \
      -DLW_PPOCR_BUILD_TESTS=ON \
      -DLW_PPOCR_BUILD_STUB_RUNTIME=ON \
      -DLW_PPOCR_BUILD_DIRECTML_RUNTIME=OFF \
      -DLW_PPOCR_BUILD_ONNXRUNTIME_RUNTIME=OFF \
      -DLW_PPOCR_BUILD_OPENVINO_RUNTIME=OFF \
      -DLW_PPOCR_BUILD_TENSORRT_RUNTIME=OFF \
      -DLW_PPOCR_TEST_DET_MODEL="${PROJECT_ROOT}/models/ppocrv6-tiny/det.onnx" \
      -DLW_PPOCR_TEST_REC_MODEL="${PROJECT_ROOT}/models/ppocrv6-tiny/rec.onnx" \
      -DLW_PPOCR_TEST_CLS_MODEL="${PROJECT_ROOT}/models/ppocrv6-tiny/cls.onnx" \
      -DLW_PPOCR_TEST_DICTIONARY="${PROJECT_ROOT}/models/ppocrv6-tiny/ppocr_keys.txt" \
      -DLW_PPOCR_TEST_IMAGE="${PROJECT_ROOT}/models/ppocrv6-tiny/sample.jpg"

    cmake --build "${BUILD_DIR}" --parallel 2
    export LD_LIBRARY_PATH="${BUILD_DIR}/bin:${OPENCV_LIB}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure --verbose
    python3 "${PROJECT_ROOT}/scripts/http-service-smoke.py" \
      --binary-dir "${BUILD_DIR}/bin"

    bash "${PROJECT_ROOT}/scripts/package-linux-arm64-opencv.sh" \
      "${BUILD_DIR}" \
      "${PROJECT_ROOT}/dist/releases/v${PACKAGE_VERSION}" \
      "${PACKAGE_VERSION}"

    PACKAGE_DIR="${PROJECT_ROOT}/dist/releases/v${PACKAGE_VERSION}/staging-linux-arm64/lw.PPOCR.Inference-v${PACKAGE_VERSION}-linux-arm64-opencv"
    python3 "${PROJECT_ROOT}/scripts/http-service-smoke.py" \
      --package-dir "${PACKAGE_DIR}"
    bash "${PACKAGE_DIR}/verify-linux-package.sh" "${PACKAGE_DIR}"

    for binary in \
      "${PACKAGE_DIR}/liblw.PPOCR.so.1" \
      "${PACKAGE_DIR}/runtimes/linux-arm64/opencv/liblw.PPOCR.Runtime.OpenCVDNN.so" \
      "${PACKAGE_DIR}/lw-ppocr-http-service"; do
      file "${binary}"
      readelf -h "${binary}" | grep 'Machine:.*AArch64'
      if ldd "${binary}" | grep -q 'not found'; then
        ldd "${binary}" >&2
        exit 1
      fi
    done
    ;;

  *)
    echo "Usage: $0 <opencv|project>" >&2
    exit 2
    ;;
esac
