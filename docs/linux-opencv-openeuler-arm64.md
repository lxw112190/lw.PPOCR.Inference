# openEuler ARM64 OpenCV DNN HTTP 服务部署

该预览包面向 **openEuler 22.03 LTS-SP1 AArch64/ARM64**，使用 OpenCV 5.0 DNN CPU 后端。CI 在原生 ARM64 Runner 上加载官方 openEuler 容器，并在该用户态环境中完成 OpenCV、Runtime、HTTP 服务的编译、真实 OCR 测试和打包。

首个预览产物名称：

```text
lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz
lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz.sha256
```

## 支持范围

- 操作系统：openEuler 22.03 LTS-SP1
- 架构：AArch64 / ARM64
- CPU 基线：ARMv8-A，不使用 `-march=native` 或特定服务器指令集
- 推理后端：OpenCV DNN 5.0，CPU
- API：完整 OCR `/api/ocr` 与单张/批量只识别 `/api/recognize`
- 部署方式：前台脚本或 systemd
- 不包含：WinForms、ARM32、GPU、特定鲲鹏/飞腾 CPU 优化

模型、字典、网页、HTTP 配置和 OpenCV 共享库均已包含在发布包中，不需要从源码仓库补充文件。

## 快速启动

将 CI Artifact ZIP 中的 `.tar.gz` 和 `.sha256` 文件复制到 ARM64 服务器：

```bash
sha256sum -c lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz
cd lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv
sudo ./install-deps-openeuler.sh
./verify-linux-package.sh
./run-http-service.sh
```

浏览器访问：

```text
http://127.0.0.1:8787/
```

`verify-linux-package.sh` 会检查包内 SHA256、AArch64 ELF 依赖、健康接口并执行一次真实 OCR。验证时请确保 8787 端口没有被其他程序占用。

## HTTP 配置

默认配置位于 `http-service.json`：

```json
{
  "listen_host": "127.0.0.1",
  "port": 8787,
  "backend": "opencv",
  "runtime_root": "runtimes/linux-arm64",
  "model_manifest": "models/ppocrv6-tiny/model.json",
  "web_root": "www",
  "enable_classifier": true,
  "worker_threads": 4,
  "api_key": ""
}
```

完整 OCR：

```bash
IMAGE_BASE64="$(base64 -w 0 test.jpg)"
curl -X POST http://127.0.0.1:8787/api/ocr \
  -H 'Content-Type: application/json' \
  --data-binary "{\"image_base64\":\"${IMAGE_BASE64}\"}"
```

客户已经裁剪好文字区域时，可直接调用只识别接口：

```bash
curl -X POST http://127.0.0.1:8787/api/recognize \
  -H 'Content-Type: application/json' \
  --data-binary "{\"image_base64\":\"${IMAGE_BASE64}\"}"
```

批量只识别请求使用 `images_base64` 数组，最多 256 张：

```json
{"images_base64":["...","..."]}
```

## API Key 与局域网访问

`api_key` 为空时认证关闭。如需局域网访问，请同时修改：

```json
{
  "listen_host": "0.0.0.0",
  "api_key": "replace-with-a-long-random-secret"
}
```

客户端在请求头中传递同一个值：

```bash
-H 'X-API-Key: replace-with-a-long-random-secret'
```

`POST /api/ocr` 和 `POST /api/recognize` 会校验 API Key，`GET /health` 保持免认证。不要把 8787 端口直接暴露到公网；应同时配置防火墙或反向代理访问控制。

## 安装为 systemd 服务

先完成前台验证和配置修改，再执行：

```bash
sudo ./install-systemd.sh
systemctl status lw-ppocr-http.service
journalctl -u lw-ppocr-http.service -f
```

程序会复制到 `/opt/lw-ppocr` 并设置为自动启动。卸载：

```bash
sudo ./uninstall-systemd.sh
```

## 常见问题

确认机器架构：

```bash
uname -m
```

必须输出 `aarch64`。如果输出 `x86_64`，应下载 Linux x64 包；该 ARM64 包不支持 ARMv7/ARM32。

检查动态库：

```bash
ldd lw-ppocr-http-service
ldd runtimes/linux-arm64/opencv/liblw.PPOCR.Runtime.OpenCVDNN.so
```

出现 `not found` 时，先执行 `sudo ./install-deps-openeuler.sh`。如果服务初始化失败，请保留启动时输出的作者信息、参数信息以及完整错误日志。

## English summary

This preview package targets openEuler 22.03 LTS-SP1 on AArch64/ARM64. It bundles the OpenCV 5.0 DNN CPU Runtime, PP-OCR models, native HTTP service, browser page, checksum verifier, and systemd scripts. Verify the archive, install the openEuler runtime dependencies, run `./verify-linux-package.sh`, and then start it with `./run-http-service.sh`. The generic ARMv8-A build intentionally avoids host-specific compiler flags. For LAN use, bind to `0.0.0.0`, configure a strong API Key, send it with `X-API-Key`, and restrict network access.
