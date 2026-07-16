# Linux OpenVINO CPU HTTP 服务部署（v1.3.0）

该包面向 **Ubuntu 20.04 x86_64**，使用官方 **OpenVINO 2025.2.0 Ubuntu 20.04** 归档构建。发布包已经包含 Loader、OpenVINO CPU Runtime、ONNX frontend、oneTBB、最小 OpenCV 5.0 共享库、PP-OCRv6 tiny 模型、HTTP 服务、测试网页和 systemd 脚本；目标机器不需要另外安装 OpenVINO SDK。

当前正式版只开放 `CPU`。项目曾在 OpenVINO GPU 路径复现 OpenCL 映射错误，因此在完成独立的 Linux Intel GPU 正确性和压力测试前，不把 GPU 混入这个稳定包。

## 1. 下载与解压

GitHub Actions Artifact ZIP 中包含：

```text
lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu.tar.gz
lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu.tar.gz.sha256
```

复制到 Ubuntu 后执行：

```bash
sha256sum -c lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu.tar.gz
cd lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu
```

OpenVINO 2025.2.0 官方文件仓库仍提供 Ubuntu 20.04 x64 专用归档。CI 使用 Ubuntu 20.04 容器和 GCC 9.4 构建，以降低在 Ubuntu 20.04/22.04 上的 glibc 兼容风险。较新的 2025.4 官方归档实际只提供 Ubuntu 22/24，因此本项目不会把 Ubuntu 22 构建误标为 Ubuntu 20 兼容包。

## 2. 安装少量系统依赖

```bash
sudo ./install-deps-ubuntu.sh
```

脚本只安装图像解码和基础运行库，不会安装或修改系统 OpenVINO。包内私有共享库只通过启动脚本的 `LD_LIBRARY_PATH` 使用，不污染全局库路径。

## 3. 完整自检

确认 8787 端口没有其他服务占用，然后执行：

```bash
./verify-linux-package.sh
```

它依次验证：

- `package-files.sha256` 中的全部包内文件；
- Loader、OpenVINO Runtime 和 HTTP 主程序的 ELF 依赖；
- `GET /health`；
- 示例图片的真实 `POST /api/ocr`；
- 返回结果与耗时字段。

看到以下信息即表示包可用：

```text
Linux package verification passed: checksum, ELF dependencies, health, OCR
```

## 4. 前台启动与网页测试

```bash
./run-http-service.sh
```

本机浏览器打开：

```text
http://127.0.0.1:8787/
```

页面支持上传图片、在原图上绘制文字区域，并分别显示图片解码、检测、方向分类、识别和 HTTP 总耗时。服务启动时会输出作者信息、配置文件、监听地址、模型路径、OpenVINO 后端参数、线程数、请求限制以及 API Key 是否启用，但不会输出 API Key 明文。

请始终用 `run-http-service.sh` 启动；直接执行二进制时若没有设置 OpenVINO 私有库目录，可能出现 `libopenvino.so` 找不到的错误。

## 5. HTTP API

### 健康检查

```bash
curl http://127.0.0.1:8787/health
```

### 完整 OCR：检测 + 分类 + 识别

```bash
IMAGE_BASE64="$(base64 -w 0 models/ppocrv6-tiny/sample.jpg)"
curl -X POST http://127.0.0.1:8787/api/ocr \
  -H 'Content-Type: application/json' \
  --data-binary "{\"image_base64\":\"${IMAGE_BASE64}\"}"
```

### 只识别已裁剪的文字区域

```bash
curl -X POST http://127.0.0.1:8787/api/recognize \
  -H 'Content-Type: application/json' \
  --data-binary "{\"image_base64\":\"${IMAGE_BASE64}\"}"
```

批量只识别：

```json
{"images_base64":["第一张图片的Base64","第二张图片的Base64"]}
```

返回项中的 `source_index` 对应原输入顺序。`/api/recognize` 不运行文字检测，适合客户已经用其他技术裁剪好文字行的场景。

## 6. API Key 与局域网访问

默认配置：

```json
"listen_host": "127.0.0.1",
"api_key": ""
```

空字符串表示不启用认证，仅适合本机测试。局域网部署时编辑 `http-service.json`：

```json
"listen_host": "0.0.0.0",
"api_key": "替换为足够长的随机密钥"
```

客户端在请求头传递：

```bash
-H 'X-API-Key: 替换为足够长的随机密钥'
```

网页顶部也有 API Key 输入框。密钥不会写入 URL，不会在刷新后保存，也不会出现在启动日志。`GET /health` 保持免认证。不要把服务直接暴露到公网；同时配置防火墙，只允许可信网段访问 8787 端口。

## 7. 关键配置

```json
{
  "backend": "openvino",
  "runtime_root": "runtimes/linux-x64",
  "model_manifest": "models/ppocrv6-tiny/model.json",
  "device_id": 0,
  "backend_options": { "device": "CPU" },
  "enable_classifier": true,
  "worker_threads": 4
}
```

| 字段 | 当前值 | 说明 |
|---|---|---|
| `backend` | `openvino` | 选择独立 OpenVINO Runtime |
| `backend_options.device` | `CPU` | 当前正式包只允许 CPU，大小写不敏感 |
| `device_id` | `0` | CPU 模式忽略该值，为 ABI 统一保留 |
| `enable_classifier` | `true` | 启用 0/180 度文字方向分类 |
| `worker_threads` | `4` | HTTP 请求工作线程数 |
| `max_request_bytes` | `20971520` | 单请求最大 20 MiB |
| `max_image_pixels` | `40000000` | 解码后最大 4000 万像素 |

OpenVINO 识别模型共享 `CompiledModel`，并创建最多 8 个推理请求；同一 OCR 引擎仍会串行保护整条流水线。高并发部署先通过 `worker_threads` 控制请求排队，如需要真正并行的多引擎实例，应由上层进程管理多个服务实例和不同端口。

## 8. 安装为 systemd 服务

先完成自检并编辑好配置，再执行：

```bash
sudo ./install-systemd.sh
systemctl status lw-ppocr-http.service
journalctl -u lw-ppocr-http.service -f
```

脚本将当前完整目录复制到 `/opt/lw-ppocr`，安装并启动 `lw-ppocr-http.service`。卸载服务：

```bash
sudo ./uninstall-systemd.sh
```

卸载脚本不自动删除 `/opt/lw-ppocr`，避免误删配置和模型。

## 9. 从其他 Linux 包切换

OpenCV、ONNX Runtime 和 OpenVINO 包的公共 C ABI 与 HTTP API 相同，但私有共享库集合不同。首次使用 OpenVINO 时应下载完整的 OpenVINO 包，不建议只向旧目录复制一个 `.so`。上层客户程序只需把后端改为 `openvino` 并使用对应 `runtimes/linux-x64/openvino` 目录，不需要改写 OCR 调用逻辑。

如果以后只发布增量更新，只有在 Release Notes 明确列出可替换文件时才覆盖旧包；模型、配置或依赖版本变化时仍应使用完整包。

## 10. 从源码构建

CI 定义位于 `.github/workflows/linux-openvino.yml`，完整流程为：

1. 恢复或构建最小 OpenCV 5.0 缓存；
2. 恢复或下载 OpenVINO 2025.2.0 Ubuntu 20.04 官方归档；
3. 构建 Loader、OpenVINO Runtime、HTTP 服务和 ABI 测试；
4. 执行真实模型完整 OCR 与只识别测试；
5. 打包模型、网页、配置、systemd、许可证和私有共享库；
6. 对打包后的目录再次执行 HTTP 冒烟测试；
7. 生成 `.tar.gz` 和 `.sha256` 并上传 Artifact。

OpenCV 与 OpenVINO 均使用 GitHub Actions cache。缓存命中时不会重新编译 OpenCV，也不会重新下载 OpenVINO；修改固定版本或缓存键才会生成新缓存。

## 11. 常见错误

- `libopenvino.so: cannot open shared object file`：请用 `./run-http-service.sh` 启动，或确认 systemd 单元中的 `LD_LIBRARY_PATH`。
- `OpenVINO backend option 'device' ...`：保持 `backend_options` 为 `{ "device": "CPU" }`。
- `Address already in use`：8787 已被占用，停止旧服务或修改端口。
- HTTP 401：已启用 API Key，请在网页或 `X-API-Key` 请求头中填写相同值。
- 校验失败：包内文件被修改或传输损坏；重新下载。编辑配置会使包内文件校验自然变化，因此应先执行一次原始包自检，再修改配置。

## English summary

The Linux OpenVINO v1.3.0 release targets Ubuntu 20.04 x86_64 and bundles the official OpenVINO 2025.2.0 CPU runtime, ONNX frontend, oneTBB, minimal OpenCV libraries, PP-OCR models, the HTTP host, browser page, checksum verifier, and systemd scripts. Verify the archive and package before editing configuration, run `sudo ./install-deps-ubuntu.sh`, then use `./run-http-service.sh`. The package intentionally accepts only `backend_options.device=CPU`; OpenVINO GPU remains disabled until a separate correctness and stress-test pass is complete. For LAN access, bind to `0.0.0.0`, configure a strong API Key, send it through `X-API-Key`, and restrict the firewall to trusted networks.
