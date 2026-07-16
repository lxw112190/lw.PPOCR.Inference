#!/usr/bin/env python3
"""Start the native HTTP service with the Stub Runtime and exercise its API."""

from __future__ import annotations

import argparse
import base64
import json
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent


def free_port() -> int:
    with socket.socket() as server:
        server.bind(("127.0.0.1", 0))
        return int(server.getsockname()[1])


def request_json(url: str, payload: object | None = None) -> object:
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="GET" if data is None else "POST",
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--binary-dir", type=Path)
    group.add_argument("--package-dir", type=Path)
    arguments = parser.parse_args()
    package_dir = arguments.package_dir.resolve() if arguments.package_dir else None
    binary_dir = (arguments.binary_dir or package_dir).resolve()
    executable = binary_dir / "lw.PPOCR.HttpService.exe"
    image = (package_dir / "models" / "ppocrv6-tiny" / "sample.jpg"
        if package_dir else PROJECT_ROOT / "models" / "ppocrv6-tiny" / "sample.jpg")
    required_paths = [executable, image]
    runtime = binary_dir / "lw.PPOCR.Runtime.Stub.dll"
    if package_dir is None:
        required_paths.append(runtime)
    for path in required_paths:
        if not path.is_file():
            raise FileNotFoundError(path)

    port = free_port()
    with tempfile.TemporaryDirectory(prefix="lw-ppocr-http-") as directory:
        config_path = Path(directory) / "http-service.json"
        if package_dir:
            config = json.loads(
                (package_dir / "http-service.json").read_text(encoding="utf-8")
            )
            config.update({
                "listen_host": "127.0.0.1",
                "port": port,
                "runtime_root": str(package_dir / "runtimes" / "win-x64"),
                "model_manifest": str(
                    package_dir / "models" / "ppocrv6-tiny" / "model.json"
                ),
                "web_root": str(package_dir / "www"),
                "api_key": "",
            })
        else:
            config = {
                "listen_host": "127.0.0.1",
                "port": port,
                "backend": "opencv",
                "runtime_root": str(binary_dir),
                "runtime_library": str(runtime),
                "model_manifest": "stub://http-smoke",
                "web_root": str(PROJECT_ROOT / "examples" / "http" / "www"),
                "enable_classifier": False,
                "worker_threads": 2,
                "max_request_bytes": 20 * 1024 * 1024,
                "max_image_pixels": 40_000_000,
            }
        config_path.write_text(json.dumps(config), encoding="utf-8")
        process = subprocess.Popen(
            [str(executable), "--console", "--config", str(config_path)],
            cwd=binary_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        try:
            health = None
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    output = process.stdout.read() if process.stdout else ""
                    raise RuntimeError(f"HTTP service exited early:\n{output}")
                try:
                    health = request_json(f"http://127.0.0.1:{port}/health")
                    break
                except (OSError, urllib.error.URLError):
                    time.sleep(0.1)
            if not isinstance(health, dict) or not health.get("ok"):
                raise RuntimeError("HTTP health check did not become ready")
            with urllib.request.urlopen(
                f"http://127.0.0.1:{port}/", timeout=10
            ) as page:
                if "lw.PPOCR 在线体验" not in page.read().decode("utf-8"):
                    raise RuntimeError("HTTP test page is missing or invalid")

            encoded = base64.b64encode(image.read_bytes()).decode("ascii")
            result = request_json(
                f"http://127.0.0.1:{port}/api/ocr",
                {"image_base64": encoded},
            )
            if not isinstance(result, dict) or not result.get("ok"):
                raise RuntimeError(f"OCR request failed: {result}")
            regions = result.get("result")
            if not isinstance(regions, list):
                raise RuntimeError(f"OCR result is not an array: {result}")
            if not isinstance(result.get("timing"), dict):
                raise RuntimeError(f"OCR timing is missing: {result}")
            if package_dir is None and regions:
                raise RuntimeError(f"Stub Runtime returned unexpected data: {result}")
            if package_dir is not None and not regions:
                raise RuntimeError(f"Production Runtime returned no regions: {result}")

            recognition = request_json(
                f"http://127.0.0.1:{port}/api/recognize",
                {"images_base64": [encoded, encoded]},
            )
            items = recognition.get("result") if isinstance(recognition, dict) else None
            if (not isinstance(recognition, dict) or not recognition.get("ok") or
                    not isinstance(items, list) or len(items) != 2 or
                    items[0].get("source_index") != 0 or
                    items[1].get("source_index") != 1 or
                    not isinstance(recognition.get("timing"), dict)):
                raise RuntimeError(
                    f"Recognition-only request failed: {recognition}")
            print(
                "HTTP service smoke passed: "
                f"backend={health.get('backend_name')}, "
                f"image={result.get('image')}, regions={len(regions)}, "
                f"recognition_items={len(items)}"
            )
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
