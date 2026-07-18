#!/usr/bin/env bash
set -euo pipefail

# Backward-compatible entry point used by the existing openEuler workflow.
export CI_DISTRO_ID="${CI_DISTRO_ID:-openeuler}"
exec bash "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/ci-linux-arm64-opencv.sh" "$@"
