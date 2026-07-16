# Linux OpenCV DNN 部署（v1.2.0）

这是项目的首个 Linux 预览版，目标环境为 **Ubuntu 20.04 x86_64**。它随包携带
CI 在 Ubuntu 20.04 中构建的最小 OpenCV 5.0 动态库并使用 DNN CPU 后端，公共 C ABI 与 Windows
v1.1 保持兼容，同时提供完整 OCR、仅识别、HTTP API、测试网页和 systemd 服务。

> v1.1.0 是 Windows 基线包，不能通过替换几个文件转换成 Linux 包。Linux 用户
> 应从 v1.2.0 Linux 完整包开始；后续 Linux 版本才采用增量替换方式。

## 快速体验

```bash
tar -xzf lw.PPOCR.Inference-v1.2.0-linux-x64-opencv.tar.gz
sha256sum -c lw.PPOCR.Inference-v1.2.0-linux-x64-opencv.tar.gz.sha256
cd lw.PPOCR.Inference-v1.2.0-linux-x64-opencv
sudo ./install-deps-ubuntu.sh
./verify-linux-package.sh
./run-http-service.sh
```

浏览器打开 `http://127.0.0.1:8787`。页面会在原图上绘制检测区域，并显示图片
解码、检测、分类、识别和服务总耗时。`POST /api/recognize` 可直接识别客户已经
裁剪好的文字区域，不会再次执行文字检测。

若需要让其他机器访问，请把 `http-service.json` 的 `listen_host` 改为局域网地址
或 `0.0.0.0`，并设置非空 `api_key`。不要把未设置 API Key 的服务直接暴露到
公网。

## 安装为 systemd 服务

```bash
sudo ./install-deps-ubuntu.sh
sudo ./install-systemd.sh
curl http://127.0.0.1:8787/health
journalctl -u lw-ppocr-http.service -f
```

安装脚本把当前包复制到 `/opt/lw-ppocr`，创建并启动
`lw-ppocr-http.service`。卸载服务使用：

```bash
sudo /opt/lw-ppocr/uninstall-systemd.sh
```

卸载脚本保留 `/opt/lw-ppocr` 中的模型和配置，避免误删用户数据。

## 从源码构建

Ubuntu 20.04 自带的 CMake 版本不足 3.24，CI 使用 pip 安装固定版本的 CMake。
正式 CI 会先构建最小 OpenCV 5.0；手动构建时也应准备 OpenCV 5.0，并通过
`OpenCV_DIR` 指向包含 `OpenCVConfig.cmake` 的目录。

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

## 文件与升级边界

- `liblw.PPOCR.so.1`：稳定 C ABI Loader。
- `runtimes/linux-x64/opencv/liblw.PPOCR.Runtime.OpenCVDNN.so`：Linux OpenCV DNN Runtime。
- `runtimes/linux-x64/opencv/libopencv_*.so.500`：随包携带的最小 OpenCV 5.0 运行库。
- `lw-ppocr-http-service`：HTTP API、网页和 systemd 的宿主程序。
- `models/ppocrv6-tiny`：ONNX 模型、字典和清单。
- `http-service.json`：用户配置；增量升级时应保留。

正式发布 v1.2.0 前，应在 Ubuntu 20.04.6 虚拟机验证压缩包校验、依赖安装、
前台启动、完整 OCR、仅识别接口、systemd 启停和重启后自启动。
`verify-linux-package.sh` 会自动检查包内文件校验、ELF 动态库依赖、健康接口和
一次完整 OCR；systemd 与开机启动仍需要在虚拟机中手动确认。

---

## English summary

The v1.2.0 Linux preview targets Ubuntu 20.04 x86_64 and bundles the minimal
OpenCV 5.0 shared libraries built by CI in an Ubuntu 20.04 container. It uses the
DNN CPU backend, preserves the public C ABI, and includes full OCR,
recognition-only inference, the HTTP API/test page, and systemd integration.
Install dependencies with `sudo ./install-deps-ubuntu.sh`, run in the foreground
with `./run-http-service.sh`, or install under `/opt/lw-ppocr` with
`sudo ./install-systemd.sh`. The Windows v1.1.0 package is not an incremental base
for Linux; v1.2.0 is the first Linux baseline.
