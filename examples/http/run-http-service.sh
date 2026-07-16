#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${SCRIPT_DIR}:${SCRIPT_DIR}/runtimes/linux-x64/opencv${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
exec "${SCRIPT_DIR}/lw-ppocr-http-service" --console \
  --config "${SCRIPT_DIR}/http-service.json"
