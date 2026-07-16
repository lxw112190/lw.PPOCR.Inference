# Linux x64 ONNX Runtime（CPU / NVIDIA CUDA）使用说明（v1.3.0）

该发行包使用官方 ONNX Runtime 1.26.0，并包含完整 OCR、仅识别接口、HTTP 服务、测试网页和 PP-OCRv6 tiny 模型。默认包内是 CPU 版 ONNX Runtime，解压后无需安装 Python，也无需安装 ONNX Runtime。

## 1. CPU 版快速启动

```bash
tar -xzf lw.PPOCR.Inference-v1.3.0-linux-x64-onnxruntime-cpu.tar.gz
cd lw.PPOCR.Inference-v1.3.0-linux-x64-onnxruntime-cpu
sudo ./install-deps-ubuntu.sh
./run-http-service.sh
```

浏览器打开 `http://127.0.0.1:8787`。服务器部署时，把 `http-service.json` 的 `listen_host` 改成 `0.0.0.0`，并配置防火墙或反向代理。

启动前可执行完整自检：

```bash
./verify-linux-package.sh
```

## 2. 推理设备配置

`http-service.json` 中的 `backend_options.device` 支持：

- `cpu`：只注册 CPU Execution Provider；CPU 发布包的默认值。
- `cuda`：必须使用 CUDA；CUDA Provider 或依赖不可用时启动失败，适合生产环境尽早暴露配置错误。
- `auto`：先创建 CUDA Session；CUDA 不可用或 Session 创建失败时自动改用 CPU。

`device_id` 是 NVIDIA CUDA 设备编号，默认 `0`。

```json
{
  "backend": "onnxruntime",
  "device_id": 0,
  "backend_options": {
    "device": "cuda"
  }
}
```

也兼容旧式布尔写法：`{"use_gpu": true}` 等价于强制 CUDA，`false` 等价于 CPU。

## 3. 将 CPU Runtime 替换为 GPU Runtime

只可使用同一 ONNX Runtime 版本和同一 Linux x64 架构。当前版本固定为 1.26.0。不要只替换 `libonnxruntime.so`；必须同时替换官方 GPU 包中的完整 `libonnxruntime*.so*` 集合，否则会缺少 CUDA Provider。

CUDA 12 示例：

```bash
tar -xzf onnxruntime-linux-x64-gpu-1.26.0.tgz
cd lw.PPOCR.Inference-v1.3.0-linux-x64-onnxruntime-cpu
mkdir -p runtimes/linux-x64/onnxruntime/cpu-backup
cp -a runtimes/linux-x64/onnxruntime/libonnxruntime*.so* \
  runtimes/linux-x64/onnxruntime/cpu-backup/
cp -a ../onnxruntime-linux-x64-gpu-1.26.0/lib/libonnxruntime*.so* \
  runtimes/linux-x64/onnxruntime/
```

然后把 `backend_options.device` 改成 `cuda` 或 `auto`。GPU 包仍包含 CPU Execution Provider，因此同一套 GPU `.so` 可运行 CPU 模式；CUDA 不支持的算子也会按 ONNX Runtime 的 Provider 优先级回落到 CPU。

ONNX Runtime 1.26.0 同时提供 CUDA 12 包 `onnxruntime-linux-x64-gpu-1.26.0.tgz` 和 CUDA 13 包 `onnxruntime-linux-x64-gpu_cuda13-1.26.0.tgz`。按服务器 CUDA、驱动和 cuDNN 环境选择，两个包不可混用。CUDA 模式还需要系统提供兼容的 NVIDIA 驱动、CUDA、cuDNN 和 zlib；其动态库目录必须能被 Linux 动态加载器找到。

## 4. HTTP API

完整 OCR：

```bash
curl -X POST http://127.0.0.1:8787/api/ocr \
  -H 'Content-Type: application/json' \
  -d '{"image_base64":"..."}'
```

仅识别已经裁剪好的文字区域：

```bash
curl -X POST http://127.0.0.1:8787/api/recognize \
  -H 'Content-Type: application/json' \
  -d '{"images_base64":["...","..."]}'
```

响应包含各阶段与总耗时；测试网页会在原图上绘制检测框。配置 `api_key` 后，请求必须携带 `X-API-Key` 请求头。不要把生产 API Key 写入公开网页或提交到仓库。

## 5. systemd 服务

```bash
sudo ./install-systemd.sh
sudo systemctl status lw-ppocr-http
```

默认安装目录为 `/opt/lw-ppocr`。修改配置后执行：

```bash
sudo systemctl restart lw-ppocr-http
```

## 6. 常见问题

- `libonnxruntime.so: cannot open shared object file`：使用 `run-http-service.sh` 启动，或把 `runtimes/linux-x64/onnxruntime` 加入 `LD_LIBRARY_PATH`。
- `libonnxruntime_providers_cuda.so` 或 CUDA/cuDNN 库缺失：确认复制了 GPU 包的完整 `.so` 集合，并核对 CUDA、cuDNN、驱动版本。
- `device=cuda` 启动失败：先改为 `cpu` 验证模型和服务，再检查 `nvidia-smi`、CUDA 和 cuDNN。
- 想允许无 GPU 的机器继续服务：使用 `device=auto`。

作者：天天代码码天天，QQ：819069052。
