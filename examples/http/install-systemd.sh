#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Please run with sudo: sudo ./install-systemd.sh" >&2
  exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="/opt/lw-ppocr"
UNIT_PATH="/etc/systemd/system/lw-ppocr-http.service"

install -d -m 0755 "${INSTALL_DIR}"
if [[ "${SCRIPT_DIR}" != "${INSTALL_DIR}" ]]; then
  cp -a "${SCRIPT_DIR}/." "${INSTALL_DIR}/"
fi
install -m 0644 "${SCRIPT_DIR}/lw-ppocr-http.service" "${UNIT_PATH}"
systemctl daemon-reload
systemctl enable --now lw-ppocr-http.service
systemctl --no-pager --full status lw-ppocr-http.service
