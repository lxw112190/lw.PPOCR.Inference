#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Please run with sudo: sudo ./install-deps-openeuler.sh" >&2
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
