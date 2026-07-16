#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "Usage: $0 <build-dir> [output-dir] [version]" >&2
  exit 2
fi

BUILD_DIR="$(cd -- "$1" && pwd)"
OUTPUT_DIR="${2:-dist/releases/v1.3.0-preview.1}"
VERSION="${3:-1.3.0-preview.1}"
PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR="$(cd -- "${OUTPUT_DIR}" && pwd)"

PACKAGE_NAME="lw.PPOCR.Inference-v${VERSION}-linux-x64-onnxruntime-cpu"
STAGING_ROOT="${OUTPUT_DIR}/staging-linux-onnxruntime-x64"
PACKAGE_DIR="${STAGING_ROOT}/${PACKAGE_NAME}"

rm -rf "${STAGING_ROOT}"
mkdir -p "${PACKAGE_DIR}"
cmake --install "${BUILD_DIR}" --prefix "${PACKAGE_DIR}"
find "${PACKAGE_DIR}/models" -type f -name '*.engine' -delete

cp "${PROJECT_ROOT}/docs/linux-onnxruntime.md" "${PACKAGE_DIR}/LINUX-README.md"
cp "${PROJECT_ROOT}/examples/http/http-service-linux-onnxruntime.json" \
  "${PACKAGE_DIR}/http-service.json"
cp "${PROJECT_ROOT}/examples/http/run-http-service-onnxruntime.sh" \
  "${PACKAGE_DIR}/run-http-service.sh"
cp "${PROJECT_ROOT}/examples/http/lw-ppocr-http-onnxruntime.service" \
  "${PACKAGE_DIR}/lw-ppocr-http.service"
chmod +x "${PACKAGE_DIR}/run-http-service.sh"

REQUIRED_FILES=(
  "liblw.PPOCR.so.1"
  "lw-ppocr-http-service"
  "http-service.json"
  "run-http-service.sh"
  "install-deps-ubuntu.sh"
  "install-systemd.sh"
  "verify-linux-package.sh"
  "www/index.html"
  "models/ppocrv6-tiny/model.json"
  "models/ppocrv6-tiny/det.onnx"
  "models/ppocrv6-tiny/rec.onnx"
  "models/ppocrv6-tiny/cls.onnx"
  "models/ppocrv6-tiny/ppocr_keys.txt"
  "licenses/onnxruntime-package-MIT.txt"
  "licenses/onnxruntime-package-ThirdPartyNotices.txt"
  "runtimes/linux-x64/onnxruntime/liblw.PPOCR.Runtime.ONNXRuntime.so"
  "runtimes/linux-x64/onnxruntime/libonnxruntime.so"
)
for required in "${REQUIRED_FILES[@]}"; do
  if [[ ! -e "${PACKAGE_DIR}/${required}" ]]; then
    echo "Required package file is missing: ${required}" >&2
    exit 1
  fi
done

for binary in \
  "${PACKAGE_DIR}/liblw.PPOCR.so.1" \
  "${PACKAGE_DIR}/runtimes/linux-x64/onnxruntime/liblw.PPOCR.Runtime.ONNXRuntime.so" \
  "${PACKAGE_DIR}/lw-ppocr-http-service"; do
  if missing_dependencies="$(ldd "${binary}" | grep 'not found')"; then
    echo "Missing shared-library dependency for ${binary}:" >&2
    echo "${missing_dependencies}" >&2
    exit 1
  fi
done

(
  cd "${PACKAGE_DIR}"
  find . -type f ! -name package-files.sha256 -print0 \
    | sort -z \
    | xargs -0 sha256sum > package-files.sha256
)

ARCHIVE="${OUTPUT_DIR}/${PACKAGE_NAME}.tar.gz"
tar -C "${STAGING_ROOT}" -czf "${ARCHIVE}" "${PACKAGE_NAME}"
(
  cd "${OUTPUT_DIR}"
  sha256sum "$(basename -- "${ARCHIVE}")" \
    > "$(basename -- "${ARCHIVE}").sha256"
)

echo "Created ${ARCHIVE}"
echo "Created ${ARCHIVE}.sha256"
