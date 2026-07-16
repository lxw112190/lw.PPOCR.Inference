#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Please run with sudo: sudo ./uninstall-systemd.sh" >&2
  exit 1
fi

systemctl disable --now lw-ppocr-http.service 2>/dev/null || true
rm -f /etc/systemd/system/lw-ppocr-http.service
systemctl daemon-reload
echo "Service removed. Runtime files remain in /opt/lw-ppocr."
