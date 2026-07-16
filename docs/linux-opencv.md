# Linux OpenCV DNN 部署（v1.3.0）

v1.3.0 Linux OpenCV DNN 包面向 **Ubuntu 20.04 x86_64**，使用 OpenCV 5.0 DNN CPU 后端。发布包已经包含 Loader、OpenCV Runtime、OpenCV 共享库、PP-OCRv6 tiny ONNX 模型、字典、HTTP 服务、测试网页和 systemd 脚本，不需要另外安装 OpenCV 或复制仓库文件。

> Windows 包不能通过替换几个文件转换成 Linux 包。Linux 用户首次部署应下载 v1.3.0 对应后端的完整包。

## 1. 获取并校验发布包

GitHub Release 应下载以下两个文件：

```text
lw.PPOCR.Inference-v1.3.0-linux-x64-opencv.tar.gz
lw.PPOCR.Inference-v1.3.0-linux-x64-opencv.tar.gz.sha256
```

从 GitHub Actions 下载的 Artifact 是一个同名 `.zip`，ZIP 内部才是上面的 `tar.gz` 和 `.sha256`。先解开外层 ZIP，再把这两个文件原样传到 Linux；不要在 Windows 解开 `tar.gz` 后重新压缩，以免丢失脚本的 Linux 可执行权限。

在 Ubuntu 中执行：

```bash
sha256sum -c lw.PPOCR.Inference-v1.3.0-linux-x64-opencv.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.3.0-linux-x64-opencv.tar.gz
cd lw.PPOCR.Inference-v1.3.0-linux-x64-opencv
```

若文件经过不保留权限的工具传输，可恢复脚本权限：

```bash
chmod +x ./*.sh
```

## 2. 安装系统依赖

OpenCV 5.0 运行库已经随包提供，只需安装它使用的少量 Ubuntu 系统库：

```bash
sudo ./install-deps-ubuntu.sh
```

脚本会安装 `curl`、`libgomp1`、JPEG、PNG、TIFF 和 zlib 运行库，不会编译 OpenCV。

## 3. 首次完整验证

首次部署建议先不要安装 systemd，也不要提前启动 HTTP 服务，直接运行：

```bash
./verify-linux-package.sh
```

该脚本依次检查：

- 包内文件 SHA-256；
- Loader、OpenCV Runtime 和 HTTP 服务的 ELF 动态库依赖；
- `GET /health`；
- 使用随包测试图片执行一次完整 OCR。

验证脚本会临时占用 `127.0.0.1:8787` 并在结束时关闭服务。如果已经安装或启动了服务，请先执行 `sudo systemctl stop lw-ppocr-http.service`，否则会因为端口占用而失败。

## 4. 前台启动与测试网页

```bash
./run-http-service.sh
```

看到以下信息表示服务已经就绪：

```text
lw.PPOCR HTTP service ready: http://127.0.0.1:8787 (OpenCV DNN)
```

浏览器打开：

```text
http://127.0.0.1:8787/
```

网页支持：

- 完整 OCR：检测、可选方向分类、识别，并在原图上绘制四点文字区域；
- 仅文字识别：直接识别已经裁剪好的文字行，不执行文字检测；
- 显示图片解码、检测、方向分类、识别、推理管线和 HTTP 总耗时；
- 服务启用 API Key 时，通过页面的 API Key 输入框发送 `X-API-Key` 请求头。

前台运行时按 `Ctrl+C` 停止。建议通过 `run-http-service.sh` 启动，因为脚本会设置包内 OpenCV 共享库所需的 `LD_LIBRARY_PATH`。

## 5. HTTP API 调用

### 健康检查

```bash
curl http://127.0.0.1:8787/health
```

### 完整 OCR

以下示例使用包内测试图片：

```bash
IMAGE_BASE64="$(base64 -w 0 models/ppocrv6-tiny/sample.jpg)"
curl --fail --silent \
  -H 'Content-Type: application/json' \
  --data-binary "{\"image_base64\":\"${IMAGE_BASE64}\"}" \
  http://127.0.0.1:8787/api/ocr
```

`POST /api/ocr` 返回识别文字、置信度、方向分类、四点坐标和各阶段耗时。

### 识别已经裁剪好的文字区域

客户已经通过其他技术获得文字行图片时，应使用 `/api/recognize`，避免再次执行文字检测：

```bash
IMAGE_BASE64="$(base64 -w 0 cropped-text-line.png)"
curl --fail --silent \
  -H 'Content-Type: application/json' \
  --data-binary "{\"image_base64\":\"${IMAGE_BASE64}\"}" \
  http://127.0.0.1:8787/api/recognize
```

批量识别最多接受 256 张图片：

```json
{"images_base64":["data:image/png;base64,...","data:image/png;base64,..."]}
```

Base64 可以是纯 Base64，也可以带 `data:image/...;base64,` 前缀。

## 6. 配置文件

前台运行读取当前目录的 `http-service.json`；systemd 安装后读取 `/opt/lw-ppocr/http-service.json`。相对路径均以配置文件所在目录为基准。

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `listen_host` | `127.0.0.1` | 监听地址；局域网访问可改为 `0.0.0.0` |
| `port` | `8787` | HTTP 端口 |
| `backend` | `opencv` | Linux v1.3.0 OpenCV 包固定使用 OpenCV DNN |
| `runtime_root` | `runtimes/linux-x64` | Runtime 根目录 |
| `model_manifest` | `models/ppocrv6-tiny/model.json` | 模型清单 |
| `web_root` | `www` | 测试网页目录 |
| `enable_classifier` | `true` | 是否执行文字方向分类 |
| `worker_threads` | `4` | HTTP 工作线程数，允许范围 1–64 |
| `max_request_bytes` | `20971520` | 单个 HTTP 请求最大字节数 |
| `max_image_pixels` | `40000000` | 解码后图片最大像素数 |
| `api_key` | 空字符串 | 空表示关闭认证；非空表示启用 |

当前 OpenCV DNN Linux 包使用 CPU。修改 `device_id` 不会启用 GPU。

## 7. API Key 与局域网访问

仅本机使用时可保留默认配置：

```json
{"listen_host":"127.0.0.1","api_key":""}
```

需要让局域网客户端访问时，编辑配置：

```json
{
  "listen_host": "0.0.0.0",
  "port": 8787,
  "api_key": "请替换为足够长的随机密钥"
}
```

客户端必须发送同一个 `X-API-Key` 请求头：

```bash
curl --fail --silent \
  -H 'Content-Type: application/json' \
  -H 'X-API-Key: 请替换为配置中的值' \
  --data-binary '{"image_base64":"..."}' \
  http://服务器IP:8787/api/ocr
```

未携带或密钥不匹配时，业务接口返回 HTTP 401；`GET /health` 保持免认证。还应配置防火墙，只允许可信网络访问，例如：

```bash
sudo ufw allow from 192.168.1.0/24 to any port 8787 proto tcp
```

不要把未配置 API Key 的服务直接暴露到公网。如需公网服务，建议在可信反向代理之后增加 HTTPS、访问控制、限流和审计。

## 8. 安装为 systemd 服务

确认前台运行正常，并在当前目录完成 `http-service.json` 配置后执行：

```bash
sudo ./install-systemd.sh
```

脚本会把完整部署包复制到 `/opt/lw-ppocr`，安装并立即启动 `lw-ppocr-http.service`，同时设置开机自动启动。

常用管理命令：

```bash
systemctl status lw-ppocr-http.service
curl http://127.0.0.1:8787/health
journalctl -u lw-ppocr-http.service -f
sudo systemctl restart lw-ppocr-http.service
sudo systemctl stop lw-ppocr-http.service
sudo systemctl start lw-ppocr-http.service
```

安装后修改配置：

```bash
sudo nano /opt/lw-ppocr/http-service.json
sudo systemctl restart lw-ppocr-http.service
```

卸载服务：

```bash
sudo /opt/lw-ppocr/uninstall-systemd.sh
```

卸载脚本只移除 systemd 单元，不删除 `/opt/lw-ppocr` 中的配置、模型和程序。

## 9. 常见问题

### `libopencv_*.so.500: cannot open shared object file`

请使用 `./run-http-service.sh` 启动，不要绕过脚本直接执行二进制；systemd 单元已经配置正确的 `LD_LIBRARY_PATH`。同时运行 `./verify-linux-package.sh` 检查归档是否完整。

### `unable to listen on configured host and port`

端口已被占用。检查并关闭旧实例，或修改 `http-service.json` 中的端口：

```bash
ss -ltnp | grep 8787
```

### 本机可访问，其他机器无法访问

确认 `listen_host` 已改为 `0.0.0.0`、服务已经重启、虚拟机使用可被宿主机访问的网络模式，并检查 Ubuntu 与云服务器安全组防火墙。

### HTTP 401、413 或图片过大

- 401：检查 `X-API-Key` 是否与配置完全一致；
- 413：请求超过 `max_request_bytes`；
- 图片像素超限：调整 `max_image_pixels`，同时评估内存占用风险。

### 性能说明

首次请求可能包含模型和内部缓存预热开销。进行性能对比时应连续调用多次，并区分响应中的解码、检测、分类、识别和服务总耗时。当前包是 CPU 基线，不包含 CUDA、DirectML、OpenVINO 或 TensorRT Linux Runtime。

## 10. 从源码构建

Ubuntu 20.04 自带的 CMake 版本低于项目要求。CI 使用固定版本的 CMake、Ninja，并构建最小 OpenCV 5.0：

```bash
sudo apt-get update
sudo apt-get install -y build-essential python3-pip
python3 -m pip install --user cmake==3.31.6 ninja==1.11.1.4
export PATH="$HOME/.local/bin:$PATH"
cmake -S . -B build/linux-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOpenCV_DIR=/path/to/opencv-5.0/lib/cmake/opencv5 \
  -DLW_PPOCR_BUILD_OPENCV_RUNTIME=ON \
  -DLW_PPOCR_BUILD_HTTP_SERVICE=ON \
  -DLW_PPOCR_BUILD_TESTS=ON \
  -DLW_PPOCR_BUILD_STUB_RUNTIME=ON \
  -DLW_PPOCR_BUILD_DIRECTML_RUNTIME=OFF \
  -DLW_PPOCR_BUILD_OPENVINO_RUNTIME=OFF \
  -DLW_PPOCR_BUILD_TENSORRT_RUNTIME=OFF
cmake --build build/linux-release --parallel
ctest --test-dir build/linux-release --output-on-failure
```

---

## English summary

The v1.3.0 Linux package targets Ubuntu 20.04 x86_64 and bundles the OpenCV 5.0 CPU runtime, ONNX models, HTTP host, browser test page, and systemd scripts. Verify the archive with `sha256sum`, run `sudo ./install-deps-ubuntu.sh`, execute `./verify-linux-package.sh` before starting another service on port 8787, and use `./run-http-service.sh` for foreground testing. Install the validated directory under `/opt/lw-ppocr` with `sudo ./install-systemd.sh`. For LAN access, set `listen_host` to `0.0.0.0`, configure a non-empty `api_key`, send it in `X-API-Key`, and restrict the port to trusted networks.
