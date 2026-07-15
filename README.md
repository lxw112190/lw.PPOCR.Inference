# lw.PPOCR.Inference

[简体中文](README.md) | [English](README_EN.md)

`lw.PPOCR.Inference` 是一个面向 Windows 的统一 PP-OCR 推理项目。项目通过稳定的 C ABI 对外提供一致接口，并将 OpenCV DNN、ONNX Runtime DirectML、OpenVINO、TensorRT 四种推理后端隔离为独立 Runtime。

上层程序只需要切换后端配置，不需要为不同推理框架重写 OCR 调用逻辑。除 C# 外，任何支持调用 C DLL 的语言都可以接入，例如 C、C++、Python、Java、Delphi、Go、Rust 等。

## 项目特点

- 一套 API 支持四种推理后端
- C ABI 不暴露 STL、`cv::Mat`、C++ 类和异常
- Runtime 及依赖库按后端隔离，避免 DLL 冲突
- 支持文本检测、方向分类、文字识别完整流程
- 支持结构化结果、JSON 结果、文字框、置信度和分阶段耗时
- 支持 BGR、RGB、BGRA、RGBA、灰度图等常见像素格式
- 提供 .NET 封装、命令行程序和 WinForms 体验程序
- 模型由 `model.json` 统一描述，应用程序无需硬编码模型文件名
- 每个引擎实例独立管理配置、线程和内存，可显式初始化与销毁

## 线程安全与稳定性

- 不同 OCR 引擎实例相互独立，可以在不同线程中同时使用。
- 同一实例允许并发调用 `Run`，具体 Runtime 可以在内部串行化推理请求。
- 结构化结果和 JSON 字符串必须使用创建它们的同一实例释放。
- `Destroy` 不能与推理并发执行；销毁前必须等待所有调用结束并释放全部结果。

`v0.2.0` 增加了 1,000 次生命周期、10,000 次连续调用、8 线程共享实例、
多实例并行，以及四个真实后端的模型压力测试。测试方法和资源增长阈值见
[稳定性测试说明](docs/stability-testing.md)。

## 四种推理后端

| 后端 | 设备 | 主要优点 | 适合场景 |
|---|---|---|---|
| OpenCV DNN | CPU | 依赖简单、兼容性好，适合作为通用 CPU 基线 | 普通桌面程序、轻量部署 |
| ONNX Runtime DirectML | CPU / GPU | 可使用 Windows DirectML 调用不同品牌显卡 | Intel、AMD、NVIDIA 通用 GPU 部署 |
| OpenVINO | CPU | Intel CPU 优化成熟，内存与并发策略稳定 | Intel CPU、高并发服务 |
| TensorRT | NVIDIA GPU | FP16 性能高、延迟低 | NVIDIA 显卡、固定部署环境 |

当前稳定策略中，OpenVINO 使用 CPU；DirectML 可在界面中选择 CPU 或 GPU；TensorRT 始终使用 NVIDIA GPU。

## 架构

```text
任意编程语言
    -> lw.PPOCR.dll                 稳定 C ABI、参数校验、Runtime 加载
        -> lw.PPOCR.Runtime.*.dll   独立推理后端
            -> OCR Core             预处理、DB 后处理、裁剪、排序、CTC 解码
            -> 推理框架             OpenCV / ORT / OpenVINO / TensorRT
```

不同 Runtime 的原生依赖分别放在：

```text
runtimes/win-x64/opencv/
runtimes/win-x64/directml/
runtimes/win-x64/openvino/
runtimes/win-x64/tensorrt/
```

这种布局可以避免 OpenCV、ONNX Runtime、OpenVINO、CUDA 和 TensorRT 的 DLL 在程序根目录互相影响。

## 快速体验

发布包中提供两个程序：

- `lw.PPOCR.Demo.exe`：WinForms 图形界面体验程序
- `lw.PPOCR.Cli.exe`：统一命令行测试程序

启动 `lw.PPOCR.Demo.exe` 后：

1. 选择推理后端。
2. 确认模型清单为 `models\ppocrv6-tiny\model.json`。
3. DirectML 后端可通过“使用 GPU”和“GPU ID”选择运行设备。
4. 根据需要勾选“方向分类 CLS”。
5. 点击“初始化”，选择图片后点击“开始识别”。
6. 切换“明细”或“纯文本”查看结果，也可以直接复制文本。

界面中的模型和 Runtime 相对路径均以 EXE 所在目录为基准，不受程序启动工作目录影响。`MainForm` 使用标准 WinForms Designer 文件组织，可直接在 Visual Studio 中编辑界面。

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

后端名称支持 `opencv`、`directml`、`openvino` 和 `tensorrt`。

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

## 公共 C API

公共头文件位于 [ppocr_api.h](include/lw/ppocr/ppocr_api.h)，核心调用流程如下：

```text
lw_ppocr_create
    -> lw_ppocr_run / lw_ppocr_run_json
        -> lw_ppocr_result_free / lw_ppocr_string_free
    -> lw_ppocr_destroy
```

所有由 DLL 返回的内存都必须使用对应的释放函数处理。不要跨模块直接 `delete` 或 `free`。

## 目录结构

```text
include/                 对外公开的 C API
src/core/                OCR 公共算法、图像处理、后处理
src/loader/              lw.PPOCR.dll 和 Runtime 加载器
src/runtime/             Runtime 私有接口
src/runtimes/            四种后端实现
bindings/dotnet/         .NET 封装、CLI 和 WinForms Demo
models/                  模型清单、Schema 和体验模型
tests/                   ABI、生命周期和后端集成测试
docs/                    架构、迁移和 TensorRT 文档
scripts/                 打包脚本
```

## 开源许可证

`lw.PPOCR.Inference` 项目代码采用 [MIT License](LICENSE)。

`third_party/clipper` 中的 Clipper 6.4.2 由 Angus Johnson 开发，采用 Boost Software License 1.0。许可证原文位于源码目录 `third_party/clipper/LICENSE.txt`，发布包中位于 `licenses/clipper-BSL-1.0.txt`。第三方组件仍遵循各自的许可证和分发条款。

## 当前状态

四种 Runtime 已完成真实模型端到端测试，统一 Loader、C ABI、.NET 封装、CLI、WinForms Demo 和 Windows 发布流程已经跑通。

本机四后端的硬件环境、SDK 版本、测试参数和多轮统计结果见 [性能测试报告](docs/performance-report.md)。

当前版本仍处于 `0.x` 阶段。在 API v1 正式冻结前，公共接口仍可能调整；冻结后将优先通过结构体大小和新增字段保持向后兼容。
