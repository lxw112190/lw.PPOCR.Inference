# HTTP API、Windows Service 与 Linux systemd

HTTP 服务基于 cpp-httplib，使用 JSON + Base64 接收图片，通过统一 C ABI 调用配置的 OCR Runtime。Windows 程序名为 `lw.PPOCR.HttpService.exe`；Linux 程序名为 `lw-ppocr-http-service`。

程序启动时先输出作者“天天代码码天天”、QQ `819069052`，随后输出监听地址、后端、Runtime、模型、线程数、请求限制和 API 参数格式。为避免泄漏凭据，API Key 只显示是否已配置，不显示明文。Windows Service 模式还会把这段信息写入 EXE 同目录的 `http-service.log`；Linux systemd 可通过 `journalctl` 查看。

## 快速使用

Windows 解压任一后端拆分包后双击 `run-http-service.cmd`；Linux 解压 OpenCV DNN、ONNX Runtime 或 OpenVINO 包后执行 `./run-http-service.sh`。浏览器访问：

Linux ONNX Runtime 配置使用 `backend: "onnxruntime"`（也接受 `ort`），并通过 `backend_options.device` 选择 `cpu`、`cuda` 或 `auto`。CPU/GPU 动态库替换与 CUDA 依赖见 [Linux ONNX Runtime 部署说明](linux-onnxruntime.md)。

Linux OpenVINO 配置使用 `backend: "openvino"` 与 `backend_options: {"device":"CPU"}`。当前预览包固定使用 OpenVINO 2025.2.0 CPU，包含私有运行库，不需要目标机器安装 OpenVINO SDK；详见 [Linux OpenVINO 部署说明](linux-openvino.md)。

```text
http://127.0.0.1:8787/
```

测试页面提供两种模式：

- 完整 OCR：检测、可选方向分类、识别，并在原图上绘制四点文字区域。
- 仅文字识别：跳过检测，直接识别客户已经裁剪好的文字行。

页面同时显示图片解码、检测、方向分类、文字识别、推理管线和服务端总耗时。

## 健康检查

```http
GET /health
```

## 完整 OCR

```http
POST /api/ocr
Content-Type: application/json

{"image_base64":"data:image/jpeg;base64,..."}
```

响应中的 `result` 是文字区域数组，包含 `text`、`score`、`cls_label`、`cls_score` 和 `x1/y1` 至 `x4/y4`。`timing` 包含原生阶段耗时与 HTTP 服务耗时：

```json
{
  "ok": true,
  "result": [
    {"text":"示例","score":0.99,"x1":10,"y1":20,"x2":80,"y2":20,"x3":80,"y3":45,"x4":10,"y4":45}
  ],
  "timing": {
    "decode_ms": 1.2,
    "detector": {"preprocess_ms":1,"inference_ms":8,"postprocess_ms":2,"total_ms":11},
    "classifier": {"preprocess_ms":0,"inference_ms":0,"postprocess_ms":0,"total_ms":0},
    "recognizer": {"preprocess_ms":1,"inference_ms":5,"postprocess_ms":1,"total_ms":7},
    "pipeline": {"preprocess_ms":0,"inference_ms":0,"postprocess_ms":0,"total_ms":19},
    "server_total_ms": 21.1
  }
}
```

## 仅文字识别

单张裁剪图：

```http
POST /api/recognize
Content-Type: application/json

{"image_base64":"data:image/png;base64,..."}
```

批量裁剪图：

```http
POST /api/recognize
Content-Type: application/json

{"images_base64":["data:image/png;base64,...","data:image/png;base64,..."]}
```

批量接口最多接受 256 张图片，结果按输入顺序返回，并通过 `source_index` 映射原始数组。该接口不执行文本检测；是否执行方向分类由服务配置的 `enable_classifier` 决定。

Base64 可以带 `data:image/...;base64,` 前缀，也可以仅传纯 Base64。Windows 使用 Windows Imaging Component 解码；Linux 使用 OpenCV `imdecode`，实际格式由系统 OpenCV 构建决定。

## 配置与安全

配置文件为程序同目录的 `http-service.json`，相对路径均以配置文件所在目录为基准。默认仅绑定 `127.0.0.1`。如需局域网访问，应同时完成以下配置：

- 将 `listen_host` 改为 `0.0.0.0`。
- 设置非空 `api_key`，客户端通过 `X-API-Key` 请求头传递。
- 配置 Windows 防火墙，并限制在可信网络内访问。

Linux 应同步配置主机防火墙；两种系统都不应把未设置 API Key 的服务直接暴露到公网。

可调限制包括 `max_request_bytes`、`max_image_pixels` 和 `worker_threads`。服务在 Base64 解码及图片解码阶段都会执行大小检查。

### API Key 的使用

API Key 功能已经实现。`http-service.json` 中的 `api_key` 为空时不验证；设置为非空字符串后，`POST /api/ocr` 和 `POST /api/recognize` 都必须携带完全一致的 `X-API-Key` 请求头，否则返回 HTTP 401。`GET /health` 保持免认证，供服务探活使用。

```bash
curl -X POST http://127.0.0.1:8787/api/ocr \
  -H "Content-Type: application/json" \
  -H "X-API-Key: 请替换成配置中的值" \
  --data-binary '{"image_base64":"..."}'
```

测试网页的 API Key 输入框使用同一个请求头，不会把 Key 放进 URL。网页刷新后不会保存输入值。如果服务未配置 API Key，该输入框可以留空。

## Windows Service

以管理员身份运行：

```powershell
lw.PPOCR.HttpService.exe --install --config http-service.json
lw.PPOCR.HttpService.exe --uninstall --config http-service.json
```

拆分包中的 `install-service.cmd` 和 `uninstall-service.cmd` 提供同样功能。服务默认使用自动（延迟）启动。不同后端可以使用不同 `service_name` 并行安装，但必须配置不同端口。

## Linux systemd

Ubuntu 20.04 的 OpenCV DNN、ONNX Runtime 与 OpenVINO 包均包含 HTTP 主程序、网页、模型、对应私有共享库和服务脚本。首次部署顺序如下：

```bash
sudo ./install-deps-ubuntu.sh
./verify-linux-package.sh
./run-http-service.sh
```

确认前台运行正常后按 `Ctrl+C` 停止，编辑当前目录的 `http-service.json`，再安装 systemd：

```bash
sudo ./install-systemd.sh
systemctl status lw-ppocr-http.service
journalctl -u lw-ppocr-http.service -f
```

安装脚本把完整目录复制到 `/opt/lw-ppocr`，服务配置位于 `/opt/lw-ppocr/http-service.json`。修改后需要重启：

```bash
sudo nano /opt/lw-ppocr/http-service.json
sudo systemctl restart lw-ppocr-http.service
curl http://127.0.0.1:8787/health
```

`uninstall-systemd.sh` 只移除 systemd 单元，保留模型、程序和配置：

```bash
sudo /opt/lw-ppocr/uninstall-systemd.sh
```

验证脚本会临时使用 8787 端口，因此应在安装或启动 systemd 之前运行。如果服务无法启动，先查看 `journalctl -u lw-ppocr-http.service -n 100 --no-pager`，再使用 `ldd` 检查包内主程序和 Runtime 是否存在 `not found`。完整说明见 [Linux OpenCV DNN](linux-opencv.md)、[Linux ONNX Runtime](linux-onnxruntime.md) 和 [Linux OpenVINO](linux-openvino.md) 部署文档。
