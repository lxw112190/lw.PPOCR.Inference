# lw.PPOCR.Inference

[简体中文](README.md) | [English](README_EN.md)

作者：**天天代码码天天**　QQ：`819069052`

`lw.PPOCR.Inference` 是一个统一 PP-OCR 推理项目。项目通过稳定的 C ABI 对外提供一致接口，并将 OpenCV DNN、ONNX Runtime DirectML、便携 ONNX Runtime、OpenVINO、TensorRT 五种推理后端隔离为独立 Runtime。Windows 保持现有四种 Runtime；Linux 正式支持 OpenCV DNN、ONNX Runtime 和 OpenVINO CPU。

当前 Linux 正式版本为 **v1.3.0**，同一个 Release 提供 OpenCV DNN、ONNX Runtime 和 OpenVINO 三个可独立部署的 HTTP 服务包。三个版本均已通过 CI 和 Ubuntu 20.04 虚拟机验证。API v1 与 ABI 继续冻结，各 Linux 后端均为独立 Runtime，不改变既有公共接口。详见 [v1.3.0 Release Notes](docs/releases/v1.3.0.md)。

v1.3.0 三个 Linux 包均面向 Ubuntu 20.04 x86_64，包含完整 OCR、单张/批量只识别、HTTP 测试网页、API Key、systemd、模型和校验文件。Windows 包不能通过替换几个文件转换成 Linux 包，各 Linux 后端首次部署也应使用对应的完整包。

| v1.3.0 Linux 正式包 | 推理设备 | 说明 | 部署文档 |
|---|---|---|---|
| `linux-x64-opencv` | CPU | 最简单的通用 CPU 基线，内置 OpenCV 5.0 | [OpenCV DNN](docs/linux-opencv.md) |
| `linux-arm64-opencv`（v1.4.0-preview.1） | CPU | openEuler 22.03 LTS-SP1 AArch64，通用 ARMv8-A | [openEuler ARM64 OpenCV DNN](docs/linux-opencv-openeuler-arm64.md) |
| `linux-x64-onnxruntime-cpu` | CPU / NVIDIA CUDA | 默认内置 ORT 1.26.0 CPU；可替换同版本完整 GPU `.so` 集合 | [ONNX Runtime](docs/linux-onnxruntime.md) |
| `linux-x64-openvino-cpu` | CPU | 内置 OpenVINO 2025.2.0，适合 Intel/AMD x64 CPU | [OpenVINO](docs/linux-openvino.md) |

三个包的 C ABI 和 HTTP API 相同，但私有共享库不能混用。Release 附件同时提供 `.tar.gz` 与 `.tar.gz.sha256`；应把压缩包上传到 Linux 后再解压，以保留脚本执行权限。

上层程序只需要切换后端配置，不需要为不同推理框架重写 OCR 调用逻辑。除 C# 外，任何支持调用 C DLL 的语言都可以接入，例如 C、C++、Python、Java、Delphi、Go、Rust 等。

## 项目特点

- 一套 API 支持五种推理后端
- C ABI 不暴露 STL、`cv::Mat`、C++ 类和异常
- Runtime 及依赖库按后端隔离，避免 DLL 冲突
- 支持文本检测、方向分类、文字识别完整流程
- 支持单张和批量“只识别”，客户可直接传入已经裁剪好的文字区域
- 支持结构化结果、JSON 结果、文字框、置信度和分阶段耗时
- 支持 BGR、RGB、BGRA、RGBA、灰度图等常见像素格式
- 提供 .NET 封装、命令行程序和 WinForms 体验程序
- 提供 JSON + Base64 HTTP API、测试网页、Docker Compose、Windows Service 和 Linux systemd 部署模式
- 模型由 `model.json` 统一描述，应用程序无需硬编码模型文件名
- 每个引擎实例独立管理配置、线程和内存，可显式初始化与销毁

## 线程安全与稳定性

- 不同 OCR 引擎实例相互独立，可以在不同线程中同时使用。
- 同一实例允许并发调用 `Run`，具体 Runtime 可以在内部串行化推理请求。
- 结构化结果和 JSON 字符串必须使用创建它们的同一实例释放。
- `Destroy` 不能与推理并发执行；销毁前必须等待所有调用结束并释放全部结果。

`v0.2.0` 增加了生命周期、长循环、并发和多实例稳定性测试；`v0.3.0` 冻结了模型清单 Schema v1 并增加了 golden 正确性测试集；`v1.0.0` 正式冻结 API v1 与 ABI。四个真实后端均完成模型压力测试，测试方法和资源增长阈值见 [稳定性测试说明](docs/stability-testing.md)。

## 五种推理后端

| 后端 | 设备 | 主要优点 | 适合场景 |
|---|---|---|---|
| OpenCV DNN | CPU | 依赖简单、兼容性好，适合作为通用 CPU 基线 | 普通桌面程序、轻量部署 |
| ONNX Runtime DirectML | CPU / GPU | 可使用 Windows DirectML 调用不同品牌显卡 | Intel、AMD、NVIDIA 通用 GPU 部署 |
| OpenVINO | CPU | Intel CPU 优化成熟，内存与并发策略稳定 | Intel CPU、高并发服务 |
| TensorRT | NVIDIA GPU | FP16 性能高、延迟低 | NVIDIA 显卡、固定部署环境 |
| ONNX Runtime（Linux） | CPU / NVIDIA CUDA | 官方 CPU/GPU 包共用接口，可配置 CPU 回退 | Linux 服务、NVIDIA GPU 部署 |

当前稳定策略中，OpenVINO 使用 CPU；DirectML 可在界面中选择 CPU 或 GPU；TensorRT 始终使用 NVIDIA GPU。

Linux OpenCV 包随附在 Ubuntu 20.04 CI 环境中构建的最小 OpenCV 5.0 动态库。独立的 Linux ONNX Runtime 包使用官方 1.26.0 CPU 包完成 CI 构建，可用同版本 GPU 包中的完整 `libonnxruntime*.so*` 集合替换后启用 CUDA。Linux OpenVINO 包固定使用官方 OpenVINO 2025.2.0 Ubuntu 20.04 归档，并以 CPU-only 方式发布。DirectML 仍仅支持 Windows。

## 架构

```text
任意编程语言
    -> lw.PPOCR.dll / liblw.PPOCR.so       稳定 C ABI、参数校验、Runtime 加载
        -> lw.PPOCR.Runtime.*.dll / .so    独立推理后端
            -> OCR Core             预处理、DB 后处理、裁剪、排序、CTC 解码
            -> 推理框架             OpenCV / ORT / OpenVINO / TensorRT
```

不同 Runtime 的原生依赖分别放在：

```text
runtimes/win-x64/opencv/
runtimes/win-x64/directml/
runtimes/win-x64/openvino/
runtimes/win-x64/tensorrt/
runtimes/linux-x64/opencv/
runtimes/linux-x64/onnxruntime/
runtimes/linux-arm64/opencv/
```

这种布局可以避免 OpenCV、ONNX Runtime、OpenVINO、CUDA 和 TensorRT 的 DLL 在程序根目录互相影响。

## 快速体验

发布包中提供三个程序：

- `lw.PPOCR.Demo.exe`：基于 .NET Framework 4.7.2 的 WinForms 图形界面体验程序
- `lw.PPOCR.Cli.exe`：基于 .NET 8 的统一命令行测试程序
- `lw.PPOCR.HttpService.exe`：原生 HTTP API 与 Windows Service

启动 `lw.PPOCR.Demo.exe` 后：

1. 选择推理后端。
2. 确认模型清单为 `models\ppocrv6-tiny\model.json`。
3. DirectML 后端可通过“使用 GPU”和“GPU ID”选择运行设备。
4. 根据需要勾选“方向分类 CLS”。
5. 点击“初始化”，选择图片后点击“开始识别”。
6. 切换“明细”或“纯文本”查看结果，也可以直接复制文本。
7. 在图片上按住鼠标左键拖拽框选文字区域，松开后会跳过检测并直接识别该区域。

界面中的模型和 Runtime 相对路径均以 EXE 所在目录为基准，不受程序启动工作目录影响。`MainForm` 使用标准 WinForms Designer 文件组织，可直接在 Visual Studio 中编辑界面。

按后端拆分的包会预选对应后端并附带体验模型。双击 `run-http-service.cmd` 可启动本机 HTTP API 和测试网页；`install-service.cmd` 可通过管理员权限安装为自动启动的 Windows 服务。API 说明见 [HTTP API 与 Windows Service](docs/http-service.md)。

Linux 包解压后执行 `sudo ./install-deps-ubuntu.sh` 和 `./run-http-service.sh`，浏览器同样访问 `http://127.0.0.1:8787`；执行 `sudo ./install-systemd.sh` 可安装为系统服务。Linux 不提供 WinForms Demo。

CI 上传的 Linux `.tar.gz` 已包含 `det/rec/cls.onnx`、字典、`model.json`、测试图片、HTML 页面、HTTP 配置、systemd 脚本和最小 OpenCV 5.0 动态库。解压后无需从仓库另外复制资源；按 [Linux 快速体验步骤](docs/linux-opencv.md#快速体验) 即可启动。

## HTTP 服务

HTTP 服务使用 JSON + Base64 接收图片，既可以执行检测、方向分类和识别的完整流程，也可以直接识别客户已经裁剪好的文字区域。服务同时提供浏览器测试页面，完整 OCR 的结果会在原图上绘制文字区域并显示各阶段耗时。

默认地址和主要接口：

```text
测试网页              GET  http://127.0.0.1:8787/
健康检查              GET  /health
完整 OCR              POST /api/ocr
单张或批量只识别      POST /api/recognize
```

完整 OCR 请求：

```json
{"image_base64":"data:image/jpeg;base64,..."}
```

已经裁剪好的单张文字区域使用 `/api/recognize`：

```json
{"image_base64":"data:image/png;base64,..."}
```

批量只识别最多接受 256 张图片：

```json
{"images_base64":["data:image/png;base64,...","data:image/png;base64,..."]}
```

### API Key

API Key 功能默认关闭。配置文件 `http-service.json` 中的 `api_key` 为空字符串时，请求不需要认证：

```json
{"api_key":""}
```

局域网或服务化部署时，建议设置一个足够长的随机字符串：

```json
{"api_key":"请替换为自己的随机密钥"}
```

启用后，`POST /api/ocr` 和 `POST /api/recognize` 必须通过 `X-API-Key` 请求头传递相同的值，否则返回 HTTP 401。`GET /health` 保持免认证，便于探活。

```bash
curl -X POST http://127.0.0.1:8787/api/ocr \
  -H "Content-Type: application/json" \
  -H "X-API-Key: 请替换为自己的随机密钥" \
  --data-binary '{"image_base64":"..."}'
```

浏览器测试页面也提供 API Key 输入框，并使用相同的 `X-API-Key` 请求头。输入值不会写入 URL，也不会在刷新后保存。服务启动日志只显示 API Key 是否启用，不输出密钥明文。

默认服务只监听 `127.0.0.1`。如需局域网访问，可把 `listen_host` 改为 `0.0.0.0`，同时必须配置 API Key、操作系统防火墙和可信网络访问规则，不建议直接暴露到公网。

Windows 使用 `run-http-service.cmd` 前台启动，或以管理员身份运行 `install-service.cmd` 安装为 Windows Service。Linux 使用 `./run-http-service.sh` 前台启动，或运行 `sudo ./install-systemd.sh` 安装到 `/opt/lw-ppocr` 并注册为 systemd 服务。更完整的配置、响应字段和部署说明见 [HTTP API、Windows Service 与 Linux systemd](docs/http-service.md)。

### Docker Compose

仓库提供 `Dockerfile` 和 `compose.yaml`。默认镜像直接下载并校验正式发布的
`v1.3.0 linux-x64-opencv` 包，模型、OpenCV DNN Runtime、HTTP 服务和测试网页均已包含，
镜像构建时不会重新编译 OpenCV：

```bash
cp .env.example .env
# 局域网或反向代理部署前，请在 .env 中设置 LW_PPOCR_API_KEY
docker compose up -d --build
docker compose logs -f http-service
```

启动后访问 `http://127.0.0.1:8787/`。默认容器面向 Linux x86_64 CPU；端口、API Key、
工作线程、自定义配置、健康检查及安全建议见 [Docker / Docker Compose 部署](docs/docker.md)。

### Linux OpenCV DNN HTTP 服务快速部署

GitHub Actions 下载的 Artifact ZIP 内含正式的 `.tar.gz` 和 `.sha256`。将这两个文件传到 Ubuntu 20.04 x86_64 后执行：

```bash
sha256sum -c lw.PPOCR.Inference-v1.3.0-linux-x64-opencv.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.3.0-linux-x64-opencv.tar.gz
cd lw.PPOCR.Inference-v1.3.0-linux-x64-opencv
sudo ./install-deps-ubuntu.sh
./verify-linux-package.sh
./run-http-service.sh
```

浏览器访问 `http://127.0.0.1:8787/`。验证脚本会检查包内校验、ELF 依赖、健康接口并执行一次真实 OCR；运行它之前不要启动另一个占用 8787 端口的服务。

正式部署前编辑 `http-service.json`。局域网访问应把 `listen_host` 改为 `0.0.0.0`、设置非空 `api_key` 并限制防火墙来源；客户端通过 `X-API-Key` 请求头传递密钥。配置完成后安装 systemd：

```bash
sudo ./install-systemd.sh
systemctl status lw-ppocr-http.service
journalctl -u lw-ppocr-http.service -f
```

完整的 API 调用、配置字段、服务管理与排错步骤见 [Linux OpenCV DNN 详细部署文档](docs/linux-opencv.md)。

### Linux ONNX Runtime CPU/CUDA HTTP 服务

正式包默认内置 ONNX Runtime 1.26.0 CPU 版，可直接运行。需要 NVIDIA CUDA 时，用官方相同版本、相同 CUDA 系列 GPU 包中的完整 `libonnxruntime*.so*` 集合替换 CPU 文件，再把 `backend_options.device` 设置为 `cuda` 或 `auto`。不要只替换一个 `.so`，也不要混用 CUDA 12 和 CUDA 13 包。完整步骤见 [Linux ONNX Runtime 部署](docs/linux-onnxruntime.md)。

```bash
sha256sum -c lw.PPOCR.Inference-v1.3.0-linux-x64-onnxruntime-cpu.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.3.0-linux-x64-onnxruntime-cpu.tar.gz
cd lw.PPOCR.Inference-v1.3.0-linux-x64-onnxruntime-cpu
sudo ./install-deps-ubuntu.sh
./verify-linux-package.sh
./run-http-service.sh
```

### Linux OpenVINO CPU HTTP 服务

CI 产物已经包含 OpenVINO 2025.2.0 CPU Runtime、oneTBB、ONNX frontend、模型、网页和 systemd 文件，解压后可直接验证：

```bash
sha256sum -c lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu.tar.gz
cd lw.PPOCR.Inference-v1.3.0-linux-x64-openvino-cpu
sudo ./install-deps-ubuntu.sh
./verify-linux-package.sh
./run-http-service.sh
```

默认只监听 `127.0.0.1:8787`，配置固定为 `"backend":"openvino"` 与 `"backend_options":{"device":"CPU"}`。当前正式包暂不开放 OpenVINO GPU。完整使用、API Key、只识别接口、systemd 和排错说明见 [Linux OpenVINO CPU 部署](docs/linux-openvino.md)。

## 命令行示例

```powershell
# OpenCV DNN CPU
.\lw.PPOCR.Cli.exe opencv models\ppocrv6-tiny\model.json test.jpg

# OpenVINO CPU
.\lw.PPOCR.Cli.exe openvino models\ppocrv6-tiny\model.json test.jpg

# DirectML，GPU 1，启用方向分类
.\lw.PPOCR.Cli.exe directml models\ppocrv6-tiny\model.json test.jpg runtimes\win-x64 1 true

# TensorRT，GPU 0，启用方向分类
.\lw.PPOCR.Cli.exe tensorrt models\ppocrv6-tiny\model.json test.jpg runtimes\win-x64 0 true
```

参数顺序如下：

```text
后端  model.json  图片  [Runtime根目录]  [GPU ID]  [是否启用CLS]  [预热次数]  [测试次数]
```

后端名称支持 `opencv`、`directml`、`openvino`、`tensorrt` 和 `onnxruntime`（别名 `ort`）。

预热次数和测试次数为可选参数。指定后，CLI 会复用同一个引擎执行多轮测试，并输出均值、中位数、P95、最小值和最大值；不指定时仍只识别一次。

## 模型清单 model.json

`model.json` 是统一模型清单，用于描述模型名称、字典、检测模型、分类模型、识别模型，以及各后端对应的模型文件。

```json
{
  "schema_version": 1,
  "name": "PP-OCRv6 tiny Chinese",
  "dictionary": {
    "path": "ppocr_keys.txt"
  },
  "stages": {
    "detector": {
      "artifacts": {
        "onnx": { "path": "det.onnx", "format": "onnx" },
        "tensorrt": { "path": "det.fp16.engine", "format": "tensorrt-engine" }
      }
    },
    "classifier": {
      "artifacts": {
        "onnx": { "path": "cls.onnx", "format": "onnx" },
        "tensorrt": { "path": "cls.fp16.engine", "format": "tensorrt-engine" }
      }
    },
    "recognizer": {
      "artifacts": {
        "onnx": { "path": "rec.onnx", "format": "onnx" },
        "tensorrt": { "path": "rec.fp16.engine", "format": "tensorrt-engine" }
      }
    }
  }
}
```

清单中的相对路径以 `model.json` 所在目录为基准。便携后端读取 ONNX 文件，TensorRT 后端读取已经转换好的 engine 文件。

完整字段定义见 [模型清单 Schema](models/model-manifest.schema.json)。

## TensorRT FP16 模型转换

TensorRT Runtime 不会在程序初始化时转换 ONNX，而是直接加载提前生成的 engine。这样可以缩短启动时间，并让模型转换错误在部署前暴露出来。

### 准备环境

1. 安装与 TensorRT 匹配的 NVIDIA 驱动和 CUDA。
2. 解压 TensorRT Windows 安装包。
3. 找到 TensorRT `bin` 目录中的 `trtexec.exe`。
4. 准备 `det.onnx`、`rec.onnx`、`cls.onnx` 和字典文件。

下面以 PP-OCRv6 tiny 模型为例。请根据自己的电脑修改两个路径变量：

```powershell
$trtexec = "C:\TensorRT-10.16.1.11\bin\trtexec.exe"
$modelDir = "C:\models\ppocrv6-tiny"
```

先确认 ONNX 模型的输入名称。当前示例模型的输入名称是 `x`；如果你的模型不是 `x`，需要修改命令中的 `x:`。

### 检测模型 det

```powershell
& $trtexec `
  --onnx="$modelDir\det.onnx" `
  --saveEngine="$modelDir\det.fp16.engine" `
  --fp16 `
  --minShapes=x:1x3x32x32 `
  --optShapes=x:1x3x960x960 `
  --maxShapes=x:1x3x960x960 `
  --builderOptimizationLevel=5 `
  --skipInference
```

检测模型的最大 shape 应覆盖程序中的 `limit_side_len`。如果需要使用大于 `960` 的检测长边，应同步提高 `maxShapes`，代价是 engine 生成时间和显存占用可能增加。

### 识别模型 rec

```powershell
& $trtexec `
  --onnx="$modelDir\rec.onnx" `
  --saveEngine="$modelDir\rec.fp16.engine" `
  --fp16 `
  --minShapes=x:1x3x48x32 `
  --optShapes=x:4x3x48x320 `
  --maxShapes=x:16x3x48x1280 `
  --builderOptimizationLevel=5 `
  --skipInference
```

这个 profile 支持最大批次 `16`、最大文字宽度 `1280`。程序运行时会读取 engine profile 的最大宽度，超宽文字裁剪会缩放到 profile 范围内，避免 `setInputShape` 失败。提高最大宽度可以减少超长文本的横向压缩，但也会增加显存需求。

### 方向分类模型 cls

```powershell
& $trtexec `
  --onnx="$modelDir\cls.onnx" `
  --saveEngine="$modelDir\cls.fp16.engine" `
  --fp16 `
  --minShapes=x:1x3x80x160 `
  --optShapes=x:8x3x80x160 `
  --maxShapes=x:16x3x80x160 `
  --builderOptimizationLevel=5 `
  --skipInference
```

如果不需要方向分类，可以不转换 cls engine，并在配置或界面中关闭 CLS。

### 转换后测试

将三个 engine 放到 `model.json` 同一目录，并确认清单文件名一致：

```text
models/ppocrv6-tiny/
  model.json
  ppocr_keys.txt
  det.onnx
  rec.onnx
  cls.onnx
  det.fp16.engine
  rec.fp16.engine
  cls.fp16.engine
```

然后执行：

```powershell
.\lw.PPOCR.Cli.exe tensorrt `
  models\ppocrv6-tiny\model.json `
  test.jpg `
  runtimes\win-x64 `
  0 `
  true
```

TensorRT engine 通常与生成它的 GPU 架构、TensorRT、CUDA 和驱动环境有关，不建议直接把一台电脑生成的 engine 当作通用模型分发。生产环境最好在部署电脑或相同硬件环境中重新生成。

更详细的 profile 说明见 [TensorRT Engine 准备文档](docs/tensorrt-engines.md)。

## 编译要求

- Windows 10/11 x64
- Visual Studio 2022，安装“使用 C++ 的桌面开发”
- CMake 3.24 或更高版本
- .NET 8 SDK
- OpenCV 5.0
- 编译 DirectML 后端时需要 ONNX Runtime DirectML 和 `DirectML.dll`
- 编译 OpenVINO 后端时需要 OpenVINO Toolkit
- 编译 TensorRT 后端时需要 CUDA Toolkit 和 TensorRT

仅构建 Loader、Stub Runtime 和基础测试：

```powershell
cmake --preset vs2022-x64
cmake --build --preset vs2022-x64-release
ctest --preset vs2022-x64-release
```

## 编译四种 Runtime

下面是 Ninja 示例。所有第三方路径都需要替换为本机实际路径：

```powershell
cmake -S . -B build\ninja-x64 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DLW_PPOCR_BUILD_OPENCV_RUNTIME=ON `
  -DLW_PPOCR_BUILD_DIRECTML_RUNTIME=ON `
  -DLW_PPOCR_BUILD_OPENVINO_RUNTIME=ON `
  -DLW_PPOCR_BUILD_TENSORRT_RUNTIME=ON `
  -DLW_PPOCR_OPENCV_ROOT=C:\SDK\opencv `
  -DLW_PPOCR_ONNXRUNTIME_DML_ROOT=C:\SDK\onnxruntime-directml `
  -DLW_PPOCR_DIRECTML_DLL=C:\SDK\DirectML.dll `
  -DLW_PPOCR_OPENVINO_ROOT=C:\SDK\openvino `
  -DLW_PPOCR_TENSORRT_ROOT=C:\SDK\TensorRT-10.16.1.11 `
  "-DCUDAToolkit_ROOT=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9"

cmake --build build\ninja-x64
ctest --test-dir build\ninja-x64 -C Release --output-on-failure
```

也可以只启用需要的 Runtime。应用程序的公共接口不会因为少编译某个后端而改变。

## 生成 Windows 发布包

先完成 C++ Runtime 编译，然后执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\package-win-x64.ps1
```

默认输出目录为：

```text
dist/lw.PPOCR.Inference-win-x64/
```

脚本会发布 Loader、Runtime、.NET CLI、WinForms Demo、示例模型和文档，并生成 `package-files.sha256`。重新打包时会保留发布目录中的 `test` 文件夹，方便保留用户自己的测试图片。

按后端拆分打包：

```powershell
.\scripts\package-win-x64.ps1 -Split
```

每个后端拆分包均包含轻量 .NET Framework 4.7.2 WinForms Demo、体验模型、原生 HTTP 服务、测试网页以及 Windows Service 安装/卸载脚本。

生成正式的统一包 ZIP 与 SHA-256：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\package-win-x64.ps1 -Archive
```

生成 core 与四个后端拆分包的版本化 ZIP 和 SHA-256：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\package-win-x64.ps1 -Split -Archive
```

正式附件输出到 `dist/releases/v1.1.0/`。

仅打包指定后端：

```powershell
.\scripts\package-win-x64.ps1 -Runtime opencv,directml
```

## 公共 C API

公共头文件位于 [ppocr_api.h](include/lw/ppocr/ppocr_api.h)，核心调用流程如下：

```text
lw_ppocr_create
    -> lw_ppocr_run / lw_ppocr_run_json
        -> lw_ppocr_result_free / lw_ppocr_string_free
    -> lw_ppocr_recognize / lw_ppocr_recognize_batch
        -> lw_ppocr_recognition_result_free
    -> lw_ppocr_destroy
```

`lw_ppocr_recognize_batch` 面向已经裁剪好的文字行，结果通过 `source_index` 保持与输入数组的对应关系。该调用不会执行 detector；是否执行方向分类由 `enable_cls` 决定。API v1 保持不变，新符号属于 v1.1.0 的追加式兼容扩展。

所有由 DLL 返回的内存都必须使用对应的释放函数处理。不要跨模块直接 `delete` 或 `free`。

## 代码示例

- [C 示例](examples/c/)：`LoadLibrary` + `GetProcAddress` 动态加载，无外部依赖
- [Python 示例](examples/python/)：`ctypes` 零依赖封装，支持 `with` 语句和 `run_file()`

## 目录结构

```text
include/                 对外公开的 C API
src/core/                OCR 公共算法、图像处理、后处理
src/loader/              lw.PPOCR.dll 和 Runtime 加载器
src/runtime/             Runtime 私有接口
src/runtimes/            四种后端实现
bindings/dotnet/         .NET 封装、CLI 和 WinForms Demo
examples/c/              C 语言示例（LoadLibrary 动态加载）
examples/python/         Python ctypes 绑定示例
models/                  模型清单、Schema 和体验模型
tests/                   ABI、生命周期、golden 和后端集成测试
docs/                    架构、迁移和 TensorRT 文档
scripts/                 打包脚本
.github/workflows/       CI 工作流
```

## openEuler ARM64 OpenCV DNN 预览包

新增的 `v1.4.0-preview.1` CI 面向 **openEuler 22.03 LTS-SP1 AArch64/ARM64**。它在 GitHub 原生 ARM64 Runner 中加载官方 openEuler 容器，使用通用 `ARMv8-A` 指令基线编译 OpenCV 5.0、Runtime 和 HTTP 服务，并执行 ABI、真实 OCR、只识别、HTTP、ELF 架构及动态库检查。

CI Artifact ZIP 内包含 `.tar.gz` 和 `.tar.gz.sha256`：

```bash
sha256sum -c lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz
cd lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv
sudo ./install-deps-openeuler.sh
./verify-linux-package.sh
./run-http-service.sh
```

浏览器访问 `http://127.0.0.1:8787/`。需要长期运行时，先修改 `http-service.json`，再执行 `sudo ./install-systemd.sh`。局域网访问应同时设置 `listen_host` 为 `0.0.0.0`、配置非空 `api_key`，并通过 `X-API-Key` 请求头传递密钥。完整说明见 [openEuler ARM64 OpenCV DNN 部署文档](docs/linux-opencv-openeuler-arm64.md)。

OpenCV 安装结果使用独立 CI 缓存；只有 OpenCV 版本、openEuler 目标、编译器/ARM 指令基线或模块集合变化时才需要重新编译。该预览包不支持 ARM32，也没有使用 `-march=native`，因此不会绑定到 CI Runner 的特定 ARM CPU。

国产 Linux ARM64 自动验证已扩展为发行版矩阵：openEuler 22.03 LTS-SP1 已通过 CI 和实体机验证；Anolis OS 8.10、OpenCloudOS 9.4 使用各自官方 ARM64 容器原生编译并执行完整打包后 OCR 测试。矩阵分别上传带 `anolis810`、`opencloudos94` 后缀的 Artifact，包内记录实际构建环境并提供 RPM 系部署脚本。银河麒麟 V10 与统信 UOS V20 预留厂商官方镜像/自托管 Runner 接入方式，不采用第三方重打包镜像作为兼容结论。详见 [国产 Linux ARM64 CI 与兼容范围](docs/linux-domestic-arm64-ci.md)。

## 开源许可证

`lw.PPOCR.Inference` 项目代码采用 [MIT License](LICENSE)。

`third_party/clipper` 中的 Clipper 6.4.2 由 Angus Johnson 开发，采用 Boost Software License 1.0。许可证原文位于源码目录 `third_party/clipper/LICENSE.txt`，发布包中位于 `licenses/clipper-BSL-1.0.txt`。第三方组件仍遵循各自的许可证和分发条款。

Windows 发布包还会在 `licenses/` 中随附 OpenCV、ONNX Runtime、DirectML、
OpenVINO、TensorRT、CUDA、cpp-httplib、nlohmann/json 和 PP-OCR 模型的许可证或第三方声明。TensorRT 与
CUDA 组件属于 NVIDIA Corporation，使用和再分发须遵守包内所指向或随附的
NVIDIA 协议；不接受相关条款时，请使用不含 NVIDIA 组件的拆分包。

## 当前状态

四种 Runtime 已完成真实模型端到端测试，统一 Loader、C ABI、.NET 封装、CLI、WinForms Demo 和 Windows 发布流程已经跑通。

本机四后端的硬件环境、SDK 版本、测试参数和多轮统计结果见 [性能测试报告](docs/performance-report.md)。

Linux v1.3.0 的 OpenCV DNN、ONNX Runtime CPU/CUDA 和 OpenVINO CPU 三个正式部署包均已完成 CI 与 Ubuntu 20.04 虚拟机验证。`LW_PPOCR_API_VERSION` 保持为 `1`，v1.0.0 ABI 冻结承诺继续有效。
