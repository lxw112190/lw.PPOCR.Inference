# lw.PPOCR.Inference 性能测试报告

## 1. 报告信息

| 项目 | 内容 |
|---|---|
| 测试日期 | 2026-07-15 22:13（UTC+08:00） |
| 测试项目 | lw.PPOCR.Inference 0.1.0，Release x64 |
| 测试后端 | OpenCV DNN、OpenVINO、ONNX Runtime DirectML、TensorRT |
| 测试目标 | 比较同一公共 API、同一 OCR 模型和同一图片下的稳态端到端推理延迟 |
| 预热与样本 | 每个后端预热 5 次，随后连续统计 30 次 |
| CLS | 开启 |

本报告记录的是当前机器、当前软件栈和当前参数下的实测结果，不代表其他硬件、驱动或模型上的保证值。

## 2. 测试机器

| 硬件 | 规格 |
|---|---|
| 机器 | ASUS TUF Gaming A15 FA507NV |
| BIOS | FA507NV.312 |
| CPU | AMD Ryzen 7 7735H，8 核 16 线程，采集时标称频率约 3.2 GHz |
| 内存 | 16 GB，操作系统可见 15.24 GiB |
| 独立显卡 | NVIDIA GeForce RTX 4060 Laptop GPU，8188 MiB 显存 |
| 集成显卡 | AMD Radeon Graphics（CPU 集成） |
| NVIDIA 驱动 | 596.36 |
| GPU 最高图形频率 | `nvidia-smi` 报告 3105 MHz |
| GPU 最高显存频率 | `nvidia-smi` 报告 8001 MHz |

DirectML 使用 `device_id=0`。当前公共能力接口还不返回 DXGI 适配器名称，因此报告只记录设备编号，不将其名称写成程序可验证结论。TensorRT 的 `device_id=0` 对应 NVIDIA RTX 4060 Laptop GPU。

## 3. 操作系统与运行状态

| 条件 | 内容 |
|---|---|
| 操作系统 | Windows 10 Enterprise 22H2 |
| 系统版本 | 10.0.19045.6456 |
| 架构 | x64 |
| 电源状态 | 外接电源，电池 100% |
| Windows 电源计划 | 平衡（Balanced） |
| CPU/GPU 锁频 | 未锁频 |
| GPU 初始状态 | P8，约 210 MHz 核心、405 MHz 显存、57°C |
| TensorRT 测试结束瞬间 | P0，约 2250 MHz 核心、8001 MHz 显存、61°C、31.26 W |
| 后台环境 | 普通 Windows 桌面环境；未主动运行其他计算任务，但未关闭所有系统服务 |

没有使用“最高性能”电源计划，也没有锁定 GPU 频率，因此结果更接近日常使用，但 GPU 动态升降频可能造成少量波动。需要实验室级复测时，应固定电源计划、温度区间和后台进程，并至少重复三轮完整测试。

## 4. SDK 与工具链版本

| 组件 | 实际版本或构建 |
|---|---|
| Visual Studio | Visual Studio Community 2022 17.10.5 |
| MSVC | 19.40.33813，x64 Release |
| CMake | 3.31.0-rc2 |
| .NET SDK | 8.0.303 |
| OpenCV | 5.0.0，`opencv_world500.dll` |
| ONNX Runtime DirectML | 1.23.0，DLL 文件版本 `1.23.20250926.7.0b2c4ac` |
| DirectML | 1.15.4，DLL 文件版本 `1.15.4+241025-1615.1.dml-1.15.fac7597` |
| OpenVINO | 2026.2.0，构建 `21903-52ddc073857` |
| TensorRT | 10.16.1.11 Windows amd64 |
| CUDA Toolkit（编译使用） | 12.6，`nvcc 12.6.20` |
| TensorRT 分发包标识 | `TensorRT-10.16.1.11.Windows.amd64.cuda-12.9` |
| NVIDIA 驱动 | 596.36 |

TensorRT 分发包名称带有 `cuda-12.9`，但本项目 CMake 配置中的 `CUDAToolkit_ROOT` 为 CUDA 12.6。报告同时保留两项，避免将“分发包标识”和“实际编译工具链”混为一谈。

## 5. 模型

测试模型为随项目提供的 **PP-OCRv6 tiny Chinese**，模型清单：

```text
models/ppocrv6-tiny/model.json
```

| 阶段 | 便携模型 | TensorRT 模型 | ONNX SHA-256 |
|---|---:|---:|---|
| det | `det.onnx`，1,780,590 B，FP32 | `det.fp16.engine`，2,899,596 B | `193bab7a04fca699a6c82e6abb5b81bdb28177f0abd4062552b04908dafb19f8` |
| cls | `cls.onnx`，1,018,940 B，FP32 | `cls.fp16.engine`，1,695,884 B | `dd8b2b61983d76ab230a58da9e0e0e84956b71c3877f2ce6e438fe22d74d2cf2` |
| rec | `rec.onnx`，4,462,639 B，FP32 | `rec.fp16.engine`，4,430,180 B | `9ef676d6ed3c88256a2d92c640c44f25b0c40947e111b14b8be8f594091563e6` |

字典为 `ppocr_keys.txt`，大小 27,153 B，SHA-256：

```text
46e1b34ef45684cb46d75ac76d355341fe7f0a2c38d6ee02e63ae6b3878019fc
```

OpenCV DNN、DirectML 和 OpenVINO 读取相同 ONNX 模型；TensorRT 读取相同模型转换得到的 FP16 engine。TensorRT engine 与 GPU、TensorRT、CUDA 和驱动环境相关，不能把本机结果直接等同于其他电脑。

## 6. 测试图片

本轮正式横向测试使用发布包内置图片：

```text
models/ppocrv6-tiny/sample.jpg
```

| 属性 | 内容 |
|---|---|
| 图片内容 | 中文洗护产品宣传图，包含多行文字和一个产品瓶图 |
| 分辨率 | 500 × 500 |
| 像素格式 | 24 位 BGR（JPEG 解码后） |
| 文件大小 | 46,855 B |
| SHA-256 | `30c417c9f758a3b62718729f5a944f7d2e10cdd2bde0e8ce6785523ddb68ffe9` |
| 检出区域 | 四个后端均为 12 个 |

这张图片与旧 Demo 使用的 `3.jpg` 是同一文件。此前用于 TensorRT 排错的高分辨率 `7.jpg` 在生成本报告时已不在工作区，因此没有把历史单次数据混入本轮统计。

## 7. OCR 参数

| 参数 | 数值 |
|---|---:|
| `limit_side_len` | 960 |
| `det_db_thresh` | 0.30 |
| `det_db_box_thresh` | 0.60 |
| `det_db_unclip_ratio` | 1.60 |
| `det_use_dilation` | false |
| `enable_cls` | true |
| `cls_threshold` | 0.90 |
| `cls_batch_size` | 8 |

识别配置使用各 Runtime 当前推荐值：

| 后端 | `rec_batch_size` | `rec_concurrency` | 精度/设备 |
|---|---:|---:|---|
| OpenCV DNN | 1 | 2 | FP32 / CPU |
| OpenVINO | 1 | 8 | ONNX 编译 / CPU |
| DirectML | 4 | 4 | FP32 / GPU 0 |
| TensorRT | 4 | 4 | FP16 / NVIDIA GPU 0 |

该对比测试的是各后端的推荐运行配置，而不是强行使用完全相同的线程数。并发配置属于整体实现的一部分，也会影响速度、内存和稳定性。

## 8. 测试方法

1. 使用 Release x64 发布包和统一 `lw.PPOCR.Cli.exe`。
2. 每个后端只创建一次引擎，模型只加载一次。
3. 图片只解码一次，并转换为 BGR24 缓冲区。
4. 先执行 5 次完整 OCR 预热，不计入统计。
5. 在同一进程和同一引擎上连续执行 30 次完整 OCR。
6. 后端之间串行测试，中间等待至少 3 秒。
7. CLS 对所有后端保持开启。
8. 统计均值、中位数、P95、最小值和最大值。

复测命令格式：

```powershell
.\lw.PPOCR.Cli.exe <backend> `
  models\ppocrv6-tiny\model.json `
  models\ppocrv6-tiny\sample.jpg `
  runtimes\win-x64 `
  0 `
  true `
  5 `
  30
```

其中最后两个参数分别为预热次数和正式统计次数。`backend` 依次使用 `opencv`、`openvino`、`directml`、`tensorrt`。

## 9. 总体结果

以下 `pipeline` 是 Runtime 报告的完整 OCR 流水线耗时。估算吞吐量按 `1000 / 平均耗时` 计算，只代表单请求连续运行的理论值。

| 后端 | 平均值 | 中位数 | P95 | 最小值 | 最大值 | 估算吞吐 | 相对 OpenCV |
|---|---:|---:|---:|---:|---:|---:|---:|
| OpenCV DNN | 108.500 ms | 108.237 ms | 111.118 ms | 105.533 ms | 115.095 ms | 9.22 次/秒 | 1.00× |
| OpenVINO | 45.892 ms | 44.779 ms | 48.579 ms | 42.952 ms | 72.850 ms | 21.79 次/秒 | 2.36× |
| DirectML | 66.559 ms | 66.354 ms | 72.218 ms | 63.276 ms | 72.789 ms | 15.02 次/秒 | 1.63× |
| TensorRT FP16 | **12.187 ms** | **11.975 ms** | **13.636 ms** | **11.359 ms** | **13.734 ms** | **82.05 次/秒** | **8.90×** |

## 10. 分阶段结果

### OpenCV DNN CPU

| 阶段 | 平均值 | 中位数 | P95 | 最小值 | 最大值 |
|---|---:|---:|---:|---:|---:|
| det | 32.419 | 32.054 | 33.837 | 31.007 | 39.847 |
| cls | 6.133 | 6.113 | 6.398 | 5.756 | 6.825 |
| rec | 69.400 | 69.381 | 72.006 | 67.500 | 72.158 |
| pipeline | 108.500 | 108.237 | 111.118 | 105.533 | 115.095 |

### OpenVINO CPU

| 阶段 | 平均值 | 中位数 | P95 | 最小值 | 最大值 |
|---|---:|---:|---:|---:|---:|
| det | 12.780 | 12.208 | 15.600 | 10.793 | 25.884 |
| cls | 10.817 | 10.624 | 11.916 | 9.767 | 16.586 |
| rec | 21.979 | 21.489 | 25.770 | 20.287 | 30.035 |
| pipeline | 45.892 | 44.779 | 48.579 | 42.952 | 72.850 |

### ONNX Runtime DirectML GPU

| 阶段 | 平均值 | 中位数 | P95 | 最小值 | 最大值 |
|---|---:|---:|---:|---:|---:|
| det | 13.345 | 13.291 | 14.539 | 12.159 | 16.178 |
| cls | 21.404 | 21.332 | 23.418 | 19.641 | 23.854 |
| rec | 31.330 | 31.653 | 33.249 | 27.880 | 37.530 |
| pipeline | 66.559 | 66.354 | 72.218 | 63.276 | 72.789 |

### TensorRT FP16 GPU

| 阶段 | 平均值 | 中位数 | P95 | 最小值 | 最大值 |
|---|---:|---:|---:|---:|---:|
| det | 3.460 | 3.362 | 4.261 | 2.821 | 5.004 |
| cls | 2.218 | 2.145 | 2.856 | 1.952 | 3.159 |
| rec | 6.163 | 6.151 | 6.688 | 5.706 | 6.991 |
| pipeline | 12.187 | 11.975 | 13.636 | 11.359 | 13.734 |

表内分阶段数据单位均为毫秒。

## 11. 调用端 Wall Time

Wall time 从 C# 调用 `OcrEngine.Run` 前开始，到结构化结果完成托管转换后结束。它包含 P/Invoke、结果复制和托管对象创建，但不包含模型初始化与 JPEG 解码。

| 后端 | Wall 平均值 | 中位数 | P95 | 最小值 | 最大值 |
|---|---:|---:|---:|---:|---:|
| OpenCV DNN | 108.621 | 108.371 | 111.323 | 105.625 | 115.182 |
| OpenVINO | 45.980 | 44.862 | 48.729 | 43.040 | 72.988 |
| DirectML | 66.648 | 66.438 | 72.331 | 63.358 | 72.898 |
| TensorRT | 12.275 | 12.074 | 13.725 | 11.428 | 13.801 |

Wall time 与 Runtime pipeline 仅相差约 0.1 ms，说明当前 .NET 封装和结果转换不是主要瓶颈。

## 12. 结果分析

1. **TensorRT FP16 最快。** 平均约 12.2 ms，相对 OpenCV DNN 提速约 8.9 倍，相对 DirectML 提速约 5.5 倍，适合固定 NVIDIA 部署环境。
2. **OpenVINO 是本机最佳 CPU 方案。** 平均约 45.9 ms，相对 OpenCV DNN 提速约 2.36 倍，不依赖独立显卡。
3. **DirectML 的优势是兼容性。** 本次延迟高于 OpenVINO CPU，但它能用统一 Windows GPU 接口覆盖不同品牌显卡，部署范围比 TensorRT 更广。
4. **OpenCV DNN 是稳定的便携基线。** 依赖和行为直观，但本图识别阶段约占 64%，整体速度明显落后于专用推理框架。
5. **TensorRT 波动最小。** 本轮 pipeline 最大值与最小值相差约 2.38 ms；OpenVINO 出现一次明显长尾，因此评价交互延迟时应关注 P95，而不是只看平均值。
6. **模型初始化不在本表中。** GPU 首次初始化、engine 反序列化和第一张图通常明显更慢，长驻服务应复用引擎，不能每张图片都初始化和销毁。

## 13. 可比性与限制

- 本报告只使用一张 500×500 图片。文字区域数量、文字宽高比、图像分辨率都会改变 det、cls、rec 的占比。
- 吞吐量是单请求连续调用估算值，不等同于多用户服务端 QPS。
- 四个后端使用各自推荐并发值，比较的是项目当前推荐配置，而不是单算子理论性能。
- TensorRT 使用 FP16，其他三个后端使用 ONNX FP32 输入模型，因此属于实际部署方案比较，不是严格的同精度内核比较。
- DirectML 的 GPU 频率、Windows 调度和共享资源会影响稳定性；TensorRT 也会受到温度、功耗墙和显卡动态频率影响。
- 笔记本 RTX 4060 的功耗配置因厂商和模式而异，不能只按 GPU 型号复制本报告结果。
- 没有锁定 CPU/GPU 频率，没有关闭所有后台服务，数据代表正常桌面环境，不是硬件实验室上限。

建议后续建立至少三类公开测试集：小图少文本、高分辨率票证、超宽多行文本，并在每类图片上统计准确率、峰值内存、初始化耗时和稳态延迟。

