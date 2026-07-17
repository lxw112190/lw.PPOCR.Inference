#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
case "$(uname -m)" in
  x86_64|amd64)
    PLATFORM_RID="linux-x64"
    ;;
  aarch64|arm64)
    PLATFORM_RID="linux-arm64"
    ;;
  *)
    echo "Unsupported Linux architecture: $(uname -m)" >&2
    exit 1
    ;;
esac

RUNTIME_DIR="${SCRIPT_DIR}/runtimes/${PLATFORM_RID}/opencv"
if [[ ! -d "${RUNTIME_DIR}" ]]; then
  echo "OpenCV Runtime directory is missing: ${RUNTIME_DIR}" >&2
  exit 1
fi
export LD_LIBRARY_PATH="${SCRIPT_DIR}:${RUNTIME_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
exec "${SCRIPT_DIR}/lw-ppocr-http-service" --console \
  --config "${SCRIPT_DIR}/http-service.json"
