#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="${1:-.}"
PACKAGE_DIR="$(cd -- "${PACKAGE_DIR}" && pwd)"

cd "${PACKAGE_DIR}"
sha256sum -c package-files.sha256

for binary in \
  liblw.PPOCR.so.1 \
  runtimes/linux-x64/opencv/liblw.PPOCR.Runtime.OpenCVDNN.so \
  lw-ppocr-http-service; do
  if ldd "${binary}" | grep -q "not found"; then
    echo "Missing shared-library dependency for ${binary}" >&2
    ldd "${binary}" >&2
    exit 1
  fi
done

LOG_FILE="$(mktemp)"
OCR_RESPONSE="$(mktemp)"
SERVICE_PID=""
cleanup() {
  if [[ -n "${SERVICE_PID}" ]]; then
    kill "${SERVICE_PID}" 2>/dev/null || true
    wait "${SERVICE_PID}" 2>/dev/null || true
  fi
  rm -f "${LOG_FILE}" "${OCR_RESPONSE}"
}
trap cleanup EXIT

./run-http-service.sh >"${LOG_FILE}" 2>&1 &
SERVICE_PID=$!
for _ in $(seq 1 100); do
  if ! kill -0 "${SERVICE_PID}" 2>/dev/null; then
    cat "${LOG_FILE}" >&2
    echo "HTTP service exited before becoming ready" >&2
    exit 1
  fi
  if curl --fail --silent http://127.0.0.1:8787/health \
      | grep -q '"ok":true'; then
    break
  fi
  sleep 0.1
done

IMAGE_BASE64="$(base64 -w 0 models/ppocrv6-tiny/sample.jpg)"
curl --fail --silent \
  -H 'Content-Type: application/json' \
  --data-binary "{\"image_base64\":\"${IMAGE_BASE64}\"}" \
  http://127.0.0.1:8787/api/ocr >"${OCR_RESPONSE}"
if ! grep -q '"ok":true' "${OCR_RESPONSE}" ||
   ! grep -q '"timing":' "${OCR_RESPONSE}"; then
  cat "${OCR_RESPONSE}" >&2
  echo "Packaged OCR request failed" >&2
  exit 1
fi

echo "Linux package verification passed: checksum, ELF dependencies, health, OCR"
