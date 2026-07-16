# 微信公众号推广稿：一个压缩包，快速部署可视化 OCR HTTP 服务

## 标题建议

主标题：

```text
一个压缩包，快速部署 OCR 服务：lw.PPOCR.Inference v1.3.0 正式发布
```

备选标题：

```text
PP-OCR Linux 部署不再折腾：OpenCV、ONNX Runtime、OpenVINO 三版本开箱即用
```

```text
从图片上传到 OCR API：一个可直接部署的 Linux 文字识别服务
```

---

## 正文

做 OCR 项目时，真正让人头疼的往往不只是“能不能识别”，而是后面的部署问题：

- C++、C#、Python、Java 等不同技术栈怎么统一调用？
- 模型、推理框架和动态库怎么放，才能不互相冲突？
- 客户机器没有开发环境，能不能解压后直接运行？
- 已经裁剪好的文字区域，能不能跳过检测直接识别？
- 能不能提供一个网页，让客户先直观看到识别效果和耗时？
- Linux 服务如何开机启动，又如何做最基本的接口认证？

围绕这些实际问题，我们完成了 `lw.PPOCR.Inference v1.3.0`。

这次正式发布 Linux OpenCV DNN、ONNX Runtime 和 OpenVINO 三个独立版本。每个版本都是完整部署包，不要求客户先安装 Python，也不需要从仓库重新编译。下载、校验、解压，就可以启动 HTTP OCR 服务。

### 一套接口，三个 Linux 推理版本

v1.3.0 同一个 Release 提供三个包：

| 版本 | 设备 | 适合场景 |
|---|---|---|
| OpenCV DNN | CPU | 部署简单、通用 CPU 基线、方便快速验证 |
| ONNX Runtime | CPU / NVIDIA CUDA | 默认 CPU；有 NVIDIA 环境时可切换 CUDA |
| OpenVINO | CPU | 注重 x64 CPU 推理效率的服务端场景 |

三个版本使用相同的 C ABI 和 HTTP API。上层程序切换推理框架时，不需要重写 OCR 调用逻辑。

推理框架的动态库被放在各自独立目录中，尽量避免 OpenCV、ONNX Runtime、OpenVINO 等组件在程序目录里相互覆盖或版本冲突。

### 不只是完整 OCR，也支持“只识别”

常规 OCR 流程包含：

```text
文字检测 → 方向分类 → 文字识别
```

HTTP 接口为：

```text
POST /api/ocr
```

但在很多真实项目中，客户已经使用摄像机 SDK、图像算法或其他检测模型裁剪好了文字区域。这时再运行一次文字检测，既浪费时间，也可能影响原有业务流程。

因此项目同时提供：

```text
POST /api/recognize
```

它可以直接识别已经裁剪好的文字图片，并支持一次提交多张图片。返回结果使用 `source_index` 对应原始输入顺序，方便批量业务处理。

### 自带浏览器测试页面

HTTP 服务启动后，浏览器访问：

```text
http://127.0.0.1:8787/
```

即可打开测试页面。

页面支持上传图片、调用 OCR，并把识别区域绘制回原图。同时显示图片解码、文字检测、方向分类、文字识别和服务端总耗时，方便开发者快速确认模型、参数与运行环境是否正常。

> 【HTTP 服务效果图放置位置】
>
> 请在这里插入 HTTP 测试网页截图，建议截图中同时包含：上传区域、原图文字框、识别结果和耗时信息。

### 解压后即可启动

以任意一个 Linux v1.3.0 包为例，将 `.tar.gz` 和 `.sha256` 上传到 Ubuntu 后执行：

```bash
sha256sum -c 文件名.tar.gz.sha256
tar -xzf 文件名.tar.gz
cd 解压后的目录

sudo ./install-deps-ubuntu.sh
./verify-linux-package.sh
./run-http-service.sh
```

`verify-linux-package.sh` 不只是检查文件是否存在，它还会验证包内 SHA-256、ELF 动态库依赖、健康接口，并使用真实示例图片执行一次 OCR。

启动后访问：

```text
http://127.0.0.1:8787/
```

需要注意：请使用 `run-http-service.sh` 启动，因为脚本会自动设置当前推理后端所需的私有动态库目录。

如果在 Windows 上解压目录后再上传到 Linux，脚本执行权限可能丢失。更推荐直接把 `.tar.gz` 上传到 Linux 后解压；必要时也可以运行：

```bash
chmod +x *.sh
```

### 支持 API Key，但不把密钥打印到日志

本机测试时，服务默认只监听：

```text
127.0.0.1:8787
```

如果需要让局域网中的其他电脑调用，可以把 `listen_host` 修改为 `0.0.0.0`，并设置一个足够长的随机 API Key。

客户端通过请求头传递：

```text
X-API-Key: 你的密钥
```

浏览器测试页面也提供 API Key 输入框。服务启动日志只显示认证是否启用，不输出密钥明文。

API Key 是轻量级访问控制，不应替代 HTTPS、反向代理和防火墙。服务不建议在没有认证和网络限制的情况下直接暴露到公网。

### 可以安装为 systemd 服务

完成测试并修改好配置后，可以安装为 Linux 系统服务：

```bash
sudo ./install-systemd.sh
systemctl status lw-ppocr-http.service
journalctl -u lw-ppocr-http.service -f
```

服务会安装到 `/opt/lw-ppocr`，支持开机启动和异常退出后自动重启。

### ONNX Runtime 的 CPU 与 CUDA

ONNX Runtime 正式包默认携带官方 1.26.0 CPU Runtime，因此没有 NVIDIA 显卡也可以直接使用。

配置支持：

```json
{"device":"cpu"}
```

```json
{"device":"cuda"}
```

```json
{"device":"auto"}
```

如果需要 CUDA，必须准备兼容的 NVIDIA 驱动、CUDA、cuDNN，并使用相同 ONNX Runtime 版本和相同 CUDA 系列官方 GPU 包中的完整 `libonnxruntime*.so*` 集合。

这里特别提醒：不要只替换一个 `libonnxruntime.so`，也不要混用 CUDA 12 和 CUDA 13 的文件。本次 v1.3.0 随附附件和 Ubuntu 虚拟机验证以 CPU Runtime 为基线。

### OpenVINO 版不需要目标机器安装 SDK

OpenVINO 包固定使用官方 OpenVINO 2025.2.0 Ubuntu 20.04 Runtime，已经携带 CPU plugin、ONNX frontend、oneTBB、hwloc 和 OCR 所需的 OpenCV 共享库。

目标机器不需要另外安装 OpenVINO SDK。

当前 v1.3.0 只开放 CPU。OpenVINO GPU 会在完成独立的正确性、驱动兼容和压力测试后再考虑加入，不在本次稳定支持范围内。

### 不同开发语言如何接入？

项目底层提供稳定的 C ABI，不向调用方暴露 STL、`cv::Mat`、C++ 类或异常。

因此除了现有的 C#、C 和 Python 示例，任何能够调用 C 动态库的语言都可以进行封装，例如：

- C / C++
- C#
- Python
- Java
- Go
- Rust
- Delphi

如果业务系统不方便直接加载动态库，也可以统一通过 HTTP API 调用。

### v1.3.0 做了哪些验证？

三个 Linux 包都已经完成：

- GitHub Actions 自动构建；
- Ubuntu 20.04 虚拟机验证；
- ABI 兼容性测试；
- 真实模型完整 OCR；
- 单张和批量只识别；
- HTTP 服务与测试网页；
- 打包后的依赖和校验验证。

项目的 `LW_PPOCR_API_VERSION` 仍然保持为 `1`，v1.0.0 的 ABI 冻结承诺继续有效。

### 下载与交流

项目与 Release 地址：

```text
【请在这里填写 GitHub 项目或 v1.3.0 Release 链接】
```

建议根据目标机器选择对应的完整包。三个后端的公共接口相同，但私有动态库不同，不建议把不同包中的 `.so` 混合复制到同一目录。

作者：**天天代码码天天**

QQ：`819069052`

如果这个项目对你的 OCR 部署有帮助，欢迎测试、反馈问题，也欢迎分享给有相同需求的朋友。

