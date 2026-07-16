# HTTP API 与 Windows Service

`lw.PPOCR.HttpService.exe` 是基于 cpp-httplib 的原生 Windows x64 服务程序，使用 JSON + Base64 接收图片，通过统一 C ABI 调用配置的 OCR Runtime。

## 快速使用

解压任一后端拆分包后，双击 `run-http-service.cmd`，浏览器访问：

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

Base64 可以带 `data:image/...;base64,` 前缀，也可以仅传纯 Base64。图片由 Windows Imaging Component 解码，支持 JPEG、PNG、BMP、GIF 和 TIFF。

## 配置与安全

配置文件为 EXE 同目录的 `http-service.json`，相对路径均以配置文件所在目录为基准。默认仅绑定 `127.0.0.1`。如需局域网访问，应同时完成以下配置：

- 将 `listen_host` 改为 `0.0.0.0`。
- 设置非空 `api_key`，客户端通过 `X-API-Key` 请求头传递。
- 配置 Windows 防火墙，并限制在可信网络内访问。

可调限制包括 `max_request_bytes`、`max_image_pixels` 和 `worker_threads`。服务在 Base64 解码及图片解码阶段都会执行大小检查。

## Windows Service

以管理员身份运行：

```powershell
lw.PPOCR.HttpService.exe --install --config http-service.json
lw.PPOCR.HttpService.exe --uninstall --config http-service.json
```

拆分包中的 `install-service.cmd` 和 `uninstall-service.cmd` 提供同样功能。服务默认使用自动（延迟）启动。不同后端可以使用不同 `service_name` 并行安装，但必须配置不同端口。
