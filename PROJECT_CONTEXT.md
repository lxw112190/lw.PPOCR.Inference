# lw.PPOCR.Inference Project Context

> 本文件是项目交接与持续开发上下文。开始修改代码前，应先阅读本文件、
> `README.md`、`docs/architecture.md` 和相关 Runtime 实现。

## 1. 项目定位

`lw.PPOCR.Inference` 是面向 Windows 的统一 PP-OCR 推理项目，目标是让任何能
调用 C DLL 的编程语言，通过同一套稳定 C ABI 使用多种推理后端。

项目不是 C# 专用库。C# 是当前第一个完整 binding 和体验程序，公共底座是：

```text
任意编程语言
    -> lw.PPOCR.dll
        -> lw.PPOCR.Runtime.OpenCVDNN.dll
        -> lw.PPOCR.Runtime.DirectML.dll
        -> lw.PPOCR.Runtime.OpenVINO.dll
        -> lw.PPOCR.Runtime.TensorRT.dll
```

维护者信息：天天代码码天天，QQ：819069052。

## 2. 当前版本与状态

| 项目 | 当前状态 |
|---|---|
| 产品版本 | `1.0.0` |
| 公共 API 版本 | `LW_PPOCR_API_VERSION = 1` — **永久冻结** |
| ABI fingerprint | `0x4C5750504F435201` |
| Windows 平台 | x64 |
| 生产 Runtime | OpenCV DNN、DirectML、OpenVINO、TensorRT |
| .NET | Binding、CLI、WinForms Demo 均已可用 |
| 示例 | C（LoadLibrary）、Python（ctypes） |
| CI | GitHub Actions（VS 构建 + ABI + golden） |
| 打包 | 统一包 + `-Split` 后端拆包 |
| 最新标签 | 待发布 `v1.0.0` |

v1.0.0 的重点是 ABI 永久冻结和长期支持承诺。详细数据见：

- `docs/abi-v1-stability.md`
- `docs/license-audit.md`
- `docs/compatibility-matrix.md`
- `docs/releases/v1.0.0.md`

## 3. 核心设计原则

1. `lw.PPOCR.dll` 是所有语言 binding 唯一调用的原生入口。
2. Loader 不链接 OpenCV、ONNX Runtime、OpenVINO、CUDA 或 TensorRT。
3. 每个推理后端是独立 Runtime DLL，依赖放在自己的目录中。
4. DLL 边界不传递 STL、`cv::Mat`、C++ 类、异常或编译器私有类型。
5. 公共和私有插件接口均使用带版本的 C struct 和函数表。
6. OCR 参数、模型、会话、线程和缓存属于引擎实例，不使用可变全局 OCR 配置。
7. 共享 OCR 预处理和后处理代码静态链接到各 Runtime。
8. `model.json` 描述模型包，应用层不硬编码模型文件名。
9. Runtime 切换不改变上层 OCR 调用流程和返回结构。
10. 版本兼容优先于局部重构；公开 struct 已发布字段不得重排或改类型。

## 4. 仓库结构

```text
lw.PPOCR.Inference/
  include/lw/ppocr/ppocr_api.h       公共 C ABI
  src/loader/                        Loader 与 Windows 版本资源
  src/runtime/include/               私有 Runtime 插件 ABI
  src/core/                          配置校验和共享 OCR Core
  src/runtimes/stub/                 ABI 测试 Runtime
  src/runtimes/opencv_dnn/           OpenCV DNN Runtime
  src/runtimes/directml/             ONNX Runtime DirectML Runtime
  src/runtimes/openvino/             OpenVINO Runtime
  src/runtimes/tensorrt/             TensorRT Runtime
  bindings/dotnet/lw.PPOCRSharp/     .NET binding
  bindings/dotnet/samples/UnifiedCli 统一命令行程序
  bindings/dotnet/samples/UnifiedDemo WinForms 体验程序
  models/                            Schema、示例清单和 V6 tiny 模型包
  tests/                             ABI 与四后端集成/稳定性测试
  scripts/package-win-x64.ps1        Windows x64 打包脚本
  docs/                              架构、测试、性能和发布文档
  dist/                              本地生成物，不进入 Git
```

解决方案文件：`lw.PPOCR.Inference.sln`。

## 5. 公共 API 与生命周期

公共头文件：`include/lw/ppocr/ppocr_api.h`。

主要调用顺序：

```text
lw_ppocr_get_version
lw_ppocr_create
lw_ppocr_get_capabilities
lw_ppocr_run / lw_ppocr_run_json
lw_ppocr_result_free / lw_ppocr_string_free
lw_ppocr_destroy
```

重要所有权规则：

- `lw_ppocr_create` 返回的 handle 由调用方持有。
- `lw_ppocr_run` 返回的结果必须用创建它的同一 handle 释放。
- `lw_ppocr_run_json` 返回的字符串必须用创建它的同一 handle 释放。
- 所有结果和字符串都释放后，才能销毁 handle。
- `lw_ppocr_destroy` 接收 handle 指针，并在成功后将其清空。
- 错误文本通过 `lw_ppocr_get_last_error` 获取，不能让 C++ 异常穿过 DLL 边界。

线程契约：

- 不同 handle 相互独立，可以并发使用。
- 同一 handle 可以收到并发 `run/run_json` 调用。
- 当前四个生产 Runtime 都在引擎级使用 `run_mutex`，同实例调用会被串行化。
- `destroy` 不是并发操作；调用方必须先等待所有推理结束并释放结果。
- 禁止一边推理一边销毁，这是调用方生命周期责任。

## 6. Runtime 后端矩阵

| 后端 | 设备 | 模型 | 当前策略 | 关键限制 |
|---|---|---|---|---|
| OpenCV DNN | CPU | ONNX | 通用 CPU 基线 | 不报告 FP16/INT8 支持 |
| DirectML | CPU/GPU | ONNX | 多识别 session、ratio bucket | 同实例外层串行；INT8 未启用 |
| OpenVINO | CPU | ONNX 编译模型 | 共享 compiled model、独立 infer request | 稳定版本禁止 GPU |
| TensorRT | NVIDIA GPU | 预生成 FP16 engine | 共享 engine、独立 context/stream/buffer | 不在程序内转模型；INT8 已移除 |

### 6.1 OpenCV DNN

- 作为依赖简单、行为直观的 CPU 基线。
- detector、classifier、recognizer 各自加载 ONNX 网络。
- 识别支持 ratio bucket 和多个 predictor。
- 引擎级 `run_mutex` 保证同实例调用安全。
- OpenCV 5.0 新图引擎可能输出 target selection warning，当前测试中不影响推理。

### 6.2 DirectML

- 基于 ONNX Runtime DirectML，可覆盖 Intel、AMD、NVIDIA GPU。
- C# Demo 必须保留“Use GPU”和“GPU ID”操作。
- `backend_options_json_utf8={"use_gpu":1}` 时启用 DML provider。
- `{"use_gpu":0}` 时使用默认 ONNX Runtime CPU provider。
- 每个识别 worker 拥有独立 ORT session，session 内使用顺序执行。
- GPU 模式关闭 memory pattern reuse，这是此前内存稳定性的重要策略。
- 同一引擎最外层仍使用 `run_mutex`，不要误认为多个调用会同时进入 GPU。
- ORT 可能将 shape 相关节点分配到 CPU，并输出节点分配 warning，这是正常提示。
- 稳定性测试必须充分预热。v0.2.0 使用 8 次预热后采集内存基线。

### 6.3 OpenVINO

- 当前稳定里程碑只允许 CPU。
- 曾复现 GPU/OpenCL `clEnqueueMapBuffer: CL_INVALID_VALUE`，不要重新默认启用 GPU。
- 识别 worker 共享 compiled model，各自持有 infer request 和临时输入缓冲区。
- recognition concurrency 最大限制为 8。
- detector/classifier 偏低延迟，recognizer 使用 throughput scheduling hint。
- 曾出现 CPU GroupConvolution `can't alloc`，并发和缓存策略必须继续做长循环验证。

### 6.4 TensorRT

- 只加载离线转换好的 engine，不在初始化过程中调用 `trtexec`。
- detector、recognizer、classifier 都使用对应 engine。
- 当前正式路径使用 FP16；INT8 及其 UI/转换逻辑已移除。
- 一个 `ICudaEngine` 可由识别 workers 共享。
- 每个 worker 独立拥有 `IExecutionContext`、非阻塞 CUDA stream、pinned host buffer
  和 device buffer。
- buffer 只在遇到更大 profile shape 时增长，随引擎销毁释放。
- engine 与 GPU 架构、TensorRT、CUDA 和驱动相关，不应跨不同机器直接分发复用。
- engine 转换说明和命令见 `README.md` 与 `docs/tensorrt-engines.md`。

## 7. OCR Core 职责

共享 Core 负责：

- 配置和图片结构校验；
- BGR、RGB、BGRA、RGBA、Gray 输入转换；
- detector 预处理和 DB 后处理；
- 文本框展开、排序、透视裁剪和旋转；
- classifier 预处理与方向判断；
- recognizer ratio bucket、批处理和结果合并；
- CTC 解码、置信度、结构化结果和 JSON；
- det/cls/rec/pipeline 分阶段计时；
- 公共结果内存构造和释放。

后端 Runtime 只实现模型加载、tensor 输入输出、设备与 SDK 特定执行逻辑，不应
复制一套独立 OCR 后处理。

## 8. 模型清单

模型入口是 `model.json`，Schema 位于：

```text
models/model-manifest.schema.json
```

清单包含：

- `schema_version`、模型名称、家族和语言；
- OCR 字典相对路径；
- detector、classifier、recognizer；
- 每阶段 input shape；
- ONNX 和 TensorRT artifact；
- artifact 格式和精度。

所有相对路径以 `model.json` 所在目录为基准。便携后端读取 ONNX artifact，
TensorRT 读取 `tensorrt-engine` artifact。不要在 Runtime 或 UI 中硬编码固定模型
文件名。

## 9. .NET Binding 与应用

### 9.1 Binding

项目：`bindings/dotnet/lw.PPOCRSharp/lw.PPOCRSharp.csproj`。

职责：

- P/Invoke 公共 C ABI；
- SafeHandle 与确定性释放；
- UTF-8、图片缓冲区和结构体封送；
- 将原生错误转换为 .NET 异常；
- 将原生结果复制为托管对象后释放原生内存。

Binding 不实现 OCR 算法。

### 9.2 CLI

程序：`lw.PPOCR.Cli.exe`。

支持后端：`opencv`、`directml`、`openvino`、`tensorrt`。支持预热次数和重复次数，
可输出平均值、中位数、P95、最小值和最大值。

### 9.3 WinForms Demo

程序：`lw.PPOCR.Demo.exe`。

- `MainForm` 使用标准 Designer 文件，方便在 Visual Studio 直接编辑。
- 模型和 Runtime 相对路径以 EXE 所在目录为基准，而不是当前工作目录。
- 后端切换、模型选择、CLS、DirectML GPU 开关、GPU ID、初始化、图片选择、
  识别、结构化/纯文本查看和复制文本均已实现。
- 图片显示使用适合窗口的缩放方式，不能裁掉测试图片主要内容。
- 界面风格保持简约、工具化，不做营销式布局。

## 10. Runtime 分发隔离

正式布局：

```text
runtimes/win-x64/
  opencv/
  directml/
  openvino/
  tensorrt/
```

各目录只放该 Runtime 和必要依赖。不要把所有 SDK DLL 混放到 EXE 根目录，
否则容易发生 OpenCV、ORT、OpenVINO、CUDA/TensorRT 的 DLL 搜索和版本冲突。

Loader 支持测试或高级嵌入场景显式传入 Runtime DLL 路径；普通应用只选择后端，
由标准目录布局定位 Runtime。

## 11. 构建环境

v0.2.0 已验证环境：

| 工具/SDK | 版本 |
|---|---|
| Visual Studio Community 2022 | 17.10.5 |
| MSVC | 19.40.33813 |
| CMake / CTest | 3.31.0-rc2 |
| .NET SDK | 8.0.303 |
| OpenCV | 5.0.0 |
| ONNX Runtime DirectML | 1.23.0 |
| DirectML | 1.15.4 |
| OpenVINO | 2026.2.0 build 21903 |
| TensorRT | 10.16.1.11 |
| CUDA Toolkit（编译） | 12.6.20 |

当前本机完整四后端构建目录是 `build/ninja-x64`。SDK 的绝对路径保存在该目录的
`CMakeCache.txt`，不应写死到提交的通用 CMake 配置中。

普通 VS preset：

```powershell
cmake --preset vs2022-x64
cmake --build --preset vs2022-x64-release
ctest --preset vs2022-x64-release
```

在普通 PowerShell 中重新编译 Ninja C++ 目标时，需要先加载 MSVC 环境：

```powershell
cmd.exe /d /c "C:\PROGRA~1\MICROS~1\2022\COMMUN~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build build\ninja-x64"
```

## 12. 测试体系

### 12.1 快速 ABI 测试

```powershell
ctest --test-dir build\ninja-x64 -C Release `
  -R "lw.ppocr.abi" --output-on-failure
```

ABI 稳定性门槛包括：

- 1,000 次 create/run/destroy；
- 10,000 次同实例连续调用；
- 8 线程共 8,000 次共享实例调用；
- 8 个并行实例；
- JSON 分配/释放；
- 非法图片；
- 私有内存和句柄检查。

### 12.2 四后端稳定性测试

```powershell
ctest --test-dir build\ninja-x64 -C Release `
  -L stability --output-on-failure
```

每个生产 Runtime 使用真实模型：预热后执行 100 次连续 OCR，再由 4 个线程执行
40 次共享实例 OCR。默认门槛是预热后私有内存增长不超过 256 MiB、句柄增长不
超过 32。

### 12.3 Golden 正确性测试

```powershell
ctest --test-dir build\ninja-x64 -C Release `
  -R lw.ppocr.golden --output-on-failure
```

使用 OpenCV DNN 在参考图片上运行 OCR，将文字、包围框坐标（±2 px）和置信度
（±0.02）与 `tests/golden/sample.jpg.golden.json` 中的录制基线逐区域比对。
设置 `LW_PPOCR_RECORD_GOLDEN=1` 可重新录制 golden 数据。

```powershell
$env:LW_PPOCR_RECORD_GOLDEN = "1"
ctest --test-dir build\ninja-x64 -C Release `
  -R lw.ppocr.golden --output-on-failure
```

可用环境变量扩展 soak test：

```powershell
$env:LW_PPOCR_STRESS_ITERATIONS = "1000"
$env:LW_PPOCR_STRESS_THREADS = "8"
$env:LW_PPOCR_STRESS_RUNS_PER_THREAD = "100"
ctest --test-dir build\ninja-x64 -C Release `
  -L integration --output-on-failure
```

当前测试边界：

- OpenCV 的 v0.2.0 完整压力测试未启用 CLS，后续应增加独立用例。
- 真实测试校验区域数一致，尚未实现文本/框坐标容差 golden test。
- `PrivateUsage` 不等于 GPU 显存，后续应加入显存采样。

## 13. 打包与发布

生成 Windows x64 分发目录：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\package-win-x64.ps1
```

输出目录：

```text
dist/lw.PPOCR.Inference-win-x64/
```

脚本执行：

1. `cmake --install` 安装 Loader、头文件、模型、Runtime 和文档；
2. `dotnet publish` 发布 CLI；
3. `dotnet publish` 发布 WinForms Demo；
4. 生成 `package-files.sha256`；
5. 保留输出目录中的 `test` 子目录，方便用户放测试图片。

v0.2.0 发布资产：

```text
dist/releases/v0.2.0/lw.PPOCR.Inference-v0.2.0-win-x64.zip
dist/releases/v0.2.0/lw.PPOCR.Inference-v0.2.0-win-x64.zip.sha256
dist/releases/v0.2.0/RELEASE_NOTES.md
```

ZIP 大小：391,692,605 B。

SHA-256：

```text
161492d1af08dfabf6e1c7c8178557eb0adc3a87bf36ce0c5a4d4039e2dc6a3d
```

发布包内部清单已验证 60 个文件全部匹配。该 ZIP 对应 `7fde528`，不要在不更新
校验文件的情况下替换 ZIP 内容。

## 14. 版本管理

修改产品版本时，至少同步检查：

- 根 `CMakeLists.txt` 的 project version；
- `include/lw/ppocr/ppocr_api.h` 的产品版本宏；
- `src/loader/loader.cpp` 的版本字符串；
- Loader 和四个生产 Runtime 的 `version.rc`；
- .NET binding、CLI、Demo 的 `.csproj`；
- 中英文 README、Release Notes 和测试报告。

Windows DLL 右键属性中的版本来自 `.rc`：

- 数字资源：`FILEVERSION`、`PRODUCTVERSION`；
- 属性页显示字符串：`FileVersion`、`ProductVersion`。

当前版本资源脚本直接包含 `windows.h`。此前使用 `winrc.h` 出现过构建环境兼容
问题，不要在没有完整编译验证的情况下改回 `winrc.h`。

产品版本、公共 API 版本、私有 Runtime API 版本和 model schema 版本相互独立。
产品小版本升级不应无理由提高 ABI 版本。

## 15. 已知约束与不要回退的决策

- 不要让 binding 直接加载四套旧 Demo DLL；必须只调用统一 Loader。
- 不要把不同 Runtime 的依赖全部复制到 EXE 根目录。
- 不要在 C ABI 中暴露 C++ 类型或由调用方直接 `delete` 原生结果。
- 不要重新默认启用 OpenVINO GPU；先解决并覆盖 OpenCL mapping failure。
- 不要恢复 TensorRT INT8 UI 或逻辑，除非有带校准数据的完整准确率验证。
- 不要在应用初始化时自动执行 `trtexec`；转换是明确的离线部署步骤。
- 不要移除 DirectML 的 GPU/CPU 选择和 GPU ID。
- 不要把同一实例的线程安全理解为并行吞吐；当前 Runtime 会串行化外层调用。
- 不要在推理仍运行或结果未释放时调用 destroy。
- 不要因任务无关而修改或提交用户的未跟踪 `.gitattributes`。
- 不要把 SDK 安装包、`dist/`、构建产物或 PDB 提交到仓库。
- 不要移除 `third_party/clipper/LICENSE.txt` 的原始许可证声明。

## 16. 当前优先级建议

1. 增加 OpenCV + CLS 的真实模型稳定性用例。
2. 增加文字、置信度和文字框坐标容差 golden test。
3. 增加多尺寸图片集：空白图、小图、票证、高分辨率、超宽文本。
4. 执行 1,000 至 10,000 次真实模型 soak test，并记录内存曲线和 GPU 显存。
5. 建立 CI：快速 ABI 门槛和具备 SDK/GPU 的 Runtime 门槛分开运行。
6. 增加 Python binding，再验证语言中立 C ABI 的可用性。
7. 为 v1.0.0 冻结 ABI、补 API 文档、错误码文档和兼容性承诺。

## 17. 开发完成检查清单

- [ ] 修改范围符合统一 Loader + 隔离 Runtime 架构。
- [ ] 未改变已发布 ABI struct 布局或内存所有权。
- [ ] 新增内存由明确对象持有，并在 destroy 时释放。
- [ ] 同实例线程行为符合 `run_mutex` 和 destroy 契约。
- [ ] 模型路径相对于 `model.json` 或 EXE 正确解析。
- [ ] 相关原生项目以 Release x64 编译通过。
- [ ] .NET binding、CLI、Demo 编译通过。
- [ ] ABI smoke/stability 测试通过。
- [ ] 受影响的真实 Runtime 集成测试通过。
- [ ] `git diff --check` 无错误。
- [ ] DLL/EXE 文件版本正确。
- [ ] 打包后内部 SHA-256 清单和外部 ZIP 校验一致。
- [ ] 未提交 SDK、模型外的大型临时文件、PDB、dist 或用户无关改动。

## 18. 关键文档索引

| 文档 | 用途 |
|---|---|
| `README.md` | 中文介绍、使用、构建和 TensorRT 转换 |
| `README_EN.md` | 英文说明 |
| `docs/architecture.md` | 架构决策和 Runtime 隔离 |
| `docs/stability-testing.md` | 稳定性门槛与 soak test |
| `docs/abi-v1-stability.md` | ABI v1 稳定性承诺和兼容规则 |
| `docs/abi-freeze-candidate.md` | ABI 候选冻结范围 |
| `docs/compatibility-matrix.md` | 四后端兼容矩阵和已知限制 |
| `docs/license-audit.md` | 第三方许可证审计报告 |
| `docs/model-manifest.md` | Schema v1 冻结规范和字段说明 |
| `docs/performance-report.md` | 四后端性能测试条件与结果 |
| `docs/tensorrt-engines.md` | TensorRT engine profile 与转换 |
| `docs/migration-roadmap.md` | 开发路线图 |
| `docs/releases/v1.0.0.md` | v1.0.0 Release Notes |
| `docs/releases/v0.9.0-rc.1.md` | v0.9.0-rc.1 Release Notes |
| `docs/releases/v0.8.0.md` | v0.8.0 Release Notes |
| `docs/releases/v0.3.0.md` | v0.3.0 Release Notes |
| `docs/releases/v0.2.0.md` | v0.2.0 Release Notes |
| `docs/releases/v0.1.0.md` | v0.1.0 Release Notes |

## 19. 当前工作区提示

截至 v1.0.0 发布：

- 分支：`main`；
- `v1.0.0` 标签待发布；
- ABI v1 已永久冻结，详见 `docs/abi-v1-stability.md`；
- 所有第三方许可证已审计，详见 `docs/license-audit.md`；
- `.gitattributes` 已纳入版本控制；
- 用户负责自行推送 GitHub，除非后续明确要求，否则不要自动 push。
