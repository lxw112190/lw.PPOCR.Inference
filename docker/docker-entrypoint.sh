#!/usr/bin/env sh
set -eu

package_root=/opt/lw-ppocr
source_config="${LW_PPOCR_CONFIG_FILE:-${package_root}/http-service.json}"
runtime_config=/tmp/http-service.json

if [ ! -r "${source_config}" ]; then
  echo "Docker configuration is not readable: ${source_config}" >&2
  exit 1
fi

require_uint() {
  name="$1"
  value="$2"
  case "${value}" in
    ''|*[!0-9]*)
      echo "${name} must be an unsigned integer: ${value}" >&2
      exit 2
      ;;
  esac
}

listen_host="${LW_PPOCR_LISTEN_HOST:-0.0.0.0}"
port="${LW_PPOCR_PORT:-8787}"
worker_threads="${LW_PPOCR_WORKER_THREADS:-4}"
max_request_bytes="${LW_PPOCR_MAX_REQUEST_BYTES:-20971520}"
max_image_pixels="${LW_PPOCR_MAX_IMAGE_PIXELS:-40000000}"
api_key="${LW_PPOCR_API_KEY:-}"

require_uint LW_PPOCR_PORT "${port}"
require_uint LW_PPOCR_WORKER_THREADS "${worker_threads}"
require_uint LW_PPOCR_MAX_REQUEST_BYTES "${max_request_bytes}"
require_uint LW_PPOCR_MAX_IMAGE_PIXELS "${max_image_pixels}"

# The released binary resolves relative resource paths from the configuration
# file directory. The generated file lives in /tmp because the container root
# filesystem is read-only, so convert package-relative paths to absolute paths.
jq \
  --arg root "${package_root}" \
  --arg listen_host "${listen_host}" \
  --argjson port "${port}" \
  --argjson worker_threads "${worker_threads}" \
  --argjson max_request_bytes "${max_request_bytes}" \
  --argjson max_image_pixels "${max_image_pixels}" \
  --arg api_key "${api_key}" \
  --from-file /usr/local/share/lw-ppocr/configure.jq \
  "${source_config}" > "${runtime_config}"

exec "${package_root}/lw-ppocr-http-service" \
  --console --config "${runtime_config}"
