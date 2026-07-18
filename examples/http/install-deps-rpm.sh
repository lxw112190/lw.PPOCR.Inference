#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Please run with sudo: sudo ./install-deps-rpm.sh" >&2
  exit 1
fi

if ! command -v dnf >/dev/null 2>&1; then
  echo "dnf is required; this script targets RPM-based server distributions" >&2
  exit 1
fi

dnf install -y \
  ca-certificates \
  curl \
  libgomp \
  libjpeg-turbo \
  libpng \
  libtiff \
  zlib

