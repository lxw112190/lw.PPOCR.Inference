# lw.PPOCR.Inference v0.2.0 完整测试报告

## 1. 报告概览

| 项目 | 内容 |
|---|---|
| 测试版本 | lw.PPOCR.Inference v0.2.0 |
| Git 提交 | `7fde5287bfed73e94e00346fe2ab785ab90a7057` |
| Git 标签 | `v0.2.0` |
| 测试日期 | 2026-07-15 23:26（UTC+08:00） |
| 构建类型 | Windows x64 Release，Ninja |
| 测试命令 | `ctest --test-dir build/ninja-x64 -C Release --output-on-failure` |
| 测试数量 | 6 |
| 最终结果 | **6/6 通过，0 失败** |
| 总耗时 | 144.59 秒 |

本报告记录 v0.2.0 发布前的 ABI、生命周期、并发、真实模型推理、资源增长、
版本信息和发布包完整性验证。它是一份稳定性与发布验收报告，不是四种后端的
性能排名或 OCR 准确率评测。

## 2. 测试目标

1. 验证公共 C ABI 的版本、函数调用、内存所有权和错误处理行为。
2. 验证引擎反复创建与销毁时没有明显的私有内存或句柄泄漏。
3. 验证同一引擎长时间重复调用和多线程调用的稳定性。
4. 验证多个引擎实例可以独立工作并正确释放。
5. 使用真实 OCR 模型验证 OpenCV DNN、DirectML、OpenVINO 和 TensorRT。
6. 检查每个后端在预热后的进程私有内存与句柄增长。
7. 核对原生 DLL、.NET 程序和最终 ZIP 的版本及 SHA-256。

## 3. 测试机器

| 硬件 | 规格 |
|---|---|
| 机器 | ASUS TUF Gaming A15 FA507NV |
| BIOS | FA507NV.312 |
| CPU | AMD Ryzen 7 7735H，8 核 16 线程 |
| 内存 | 16 GB，操作系统可见约 15.24 GiB |
| 独立显卡 | NVIDIA GeForce RTX 4060 Laptop GPU，8188 MiB 显存 |
| 集成显卡 | AMD Radeon Graphics |
| NVIDIA 驱动 | 596.36 |

| 环境 | 状态 |
|---|---|
| 操作系统 | Windows 10 Enterprise 22H2，10.0.19045.6456，x64 |
| 电源 | 外接电源，Windows 平衡计划 |
| CPU/GPU 锁频 | 未锁频 |
| 后台状态 | 普通 Windows 桌面环境，未关闭全部系统服务 |

本机未锁定 GPU 频率，因此测试耗时会受到动态升降频、温度和 Windows 调度
影响；稳定性判定不依赖单次推理耗时。

## 4. SDK 与工具链

| 组件 | 版本 |
|---|---|
| Visual Studio | Visual Studio Community 2022 17.10.5 |
| MSVC | 19.40.33813，工具目录 14.40.33807 |
| CMake / CTest | 3.31.0-rc2 |
| .NET SDK | 8.0.303 |
| OpenCV | 5.0.0，`opencv_world500.dll` |
| ONNX Runtime DirectML | 1.23.0 |
| DirectML | 1.15.4 |
| OpenVINO | 2026.2.0，构建 `21903-52ddc073857` |
| TensorRT | 10.16.1.11 Windows amd64 |
| CUDA Toolkit（编译） | 12.6.20 |
| TensorRT 分发包 | `TensorRT-10.16.1.11.Windows.amd64.cuda-12.9` |

TensorRT 分发包名称中的 CUDA 12.9 是安装包标识；本项目此次实际编译使用的
`CUDAToolkit_ROOT` 为 CUDA 12.6，两者在报告中分别记录。

## 5. 测试模型与图片

### 5.1 OpenCV、DirectML、OpenVINO

三个后端读取同一套 PP-OCRv5 ONNX 文件：

| 文件 | 大小 | SHA-256 |
|---|---:|---|
| `PP-OCRv5_mobile_det_onnx.onnx` | 4,826,518 B | `1eb7b4f7ab657ebd1c66d5f79bca7497f29768a2e3c15e52daecbba1a8e4a039` |
| `PP-OCRv5_mobile_rec_onnx.onnx` | 16,560,873 B | `f2fb81dc0cf6bf07736e7422bab38c6636e776bc8b5bc8c8d3c7d7322cd8f3a9` |
| `PP-OCRv5_mobile_cls_onnx.onnx` | 1,018,940 B | `dd8b2b61983d76ab230a58da9e0e0e84956b71c3877f2ce6e438fe22d74d2cf2` |
| `ppocrv5_dict.txt` | 74,012 B | `d1979e9f794c464c0d2e0b70a7fe14dd978e9dc644c0e71f14158cdf8342af1b` |

### 5.2 TensorRT

TensorRT 使用已经转换好的 FP16 engine：

| 文件 | 大小 | SHA-256 |
|---|---:|---|
| `PP-OCRv6_tiny_det_fp16.engine` | 2,899,596 B | `cb425f77f5c09532547b22f7434bdbce7369adf381c61e5ca3b1125092ca1597` |
| `PP-OCRv6_tiny_rec_fp16.engine` | 4,430,180 B | `2fe2c4a4fb8aaaa6a91be781d62cfa954a1fb57f9e1a16d27cdf7259d796082a` |
| `PP-OCRv5_mobile_cls_onnx.engine` | 1,695,884 B | `0b371efadec3fad22f031e63eb1038ee0170de05a29784df93d81377ac3eab6c` |

TensorRT engine 与 GPU 架构、TensorRT、CUDA 和驱动环境相关，不保证可以在
不同硬件环境直接复用。

### 5.3 测试图片

| 属性 | 内容 |
|---|---|
| 文件 | `1.jpg` |
| 内容 | 中华人民共和国机动车驾驶证，包含中英文多行文字 |
| 分辨率 | 698 x 476 |
| 解码格式 | 24 位 BGR |
| 文件大小 | 179,561 B |
| SHA-256 | `3e7862252fcf9b8af500b11c95def8d9beb8938deccb6bb35253086350f84f4b` |
| 预期区域数 | 22 |

四个后端均返回 22 个文字区域。PP-OCRv5 与 PP-OCRv6 的部分置信度存在正常
差异，本次稳定性门槛校验区域数量、调用成功和资源回收，不把不同模型版本的
置信度强制设为完全一致。

## 6. 公共参数

| 参数 | 值 |
|---|---:|
| `limit_side_len` | 960 |
| `det_db_thresh` | 0.30 |
| `det_db_box_thresh` | 0.60 |
| `det_db_unclip_ratio` | 1.60 |
| `det_use_dilation` | false |
| `cls_threshold` | 0.90 |
| `cls_batch_size` | 8 |

| 后端 | 设备 | `rec_batch_size` | `rec_concurrency` | CLS |
|---|---|---:|---:|---|
| OpenCV DNN | CPU | 4 | 2 | 本测试未启用 |
| DirectML | GPU，`device_id=1` | 4 | 4 | 启用 |
| OpenVINO | CPU | 1 | 8 | 启用 |
| TensorRT | NVIDIA GPU 0 | 4 | 4 | 启用 |

OpenCV 测试命令没有传入分类模型，因此本轮没有覆盖 OpenCV CLS 路径；其余
三个真实后端启用了 CLS。这是后续测试矩阵需要补齐的项目。

## 7. 测试设计

### 7.1 ABI 冒烟测试

- 通过公共 Loader 加载 Stub Runtime。
- 检查 API 版本、创建、推理、JSON、错误获取、结果释放与销毁。
- 验证运行时返回 `lw.PPOCR.Inference 0.2.0`。

### 7.2 ABI 稳定性测试

- 1,000 次 `create -> run -> destroy` 生命周期循环。
- 同一持久实例执行 10,000 次连续调用。
- 8 个线程共享同一实例，每线程调用 1,000 次，共 8,000 次。
- 同时创建 8 个独立实例，并在不同线程中调用。
- 每 100 次调用执行一次 JSON 结果分配与释放。
- 验证非法图片缓冲区被拒绝且不返回结果。
- 检查 Windows 进程私有内存和句柄数量。

Stub 测试阈值：私有内存增长不超过 16 MiB，句柄增长不超过 8。

### 7.3 四后端真实模型测试

每个后端执行以下步骤：

1. 生成临时 `model.json` 并通过公共 Loader 初始化 Runtime。
2. 查询并核对 Runtime 能力。
3. 加速后端执行 8 次预热；OpenCV 在正式结果前执行一次推理。
4. 执行一次推理，检查状态、结果指针和 22 个区域。
5. 记录预热后的进程私有内存和句柄基线。
6. 在同一实例上连续执行 100 次完整 OCR。
7. 使用 4 个线程共享同一实例，每线程执行 10 次，共 40 次。
8. 每次调用均检查区域数并释放结构化结果。
9. 再次采集资源，检查增长阈值，最后销毁实例。

真实 Runtime 阈值：预热后私有内存增长不超过 256 MiB，句柄增长不超过 32。
同一实例的并发调用允许由 Runtime 内部串行化，因此该测试验证线程安全和资源
回收，不代表 4 线程吞吐性能。

## 8. 最终测试结果

| # | CTest 名称 | 内容 | 耗时 | 结果 |
|---:|---|---|---:|---|
| 1 | `lw.ppocr.abi.smoke` | ABI 基础功能 | 0.02 s | 通过 |
| 2 | `lw.ppocr.abi.stability` | 生命周期、长循环、并发、多实例 | 0.29 s | 通过 |
| 3 | `lw.ppocr.directml.integration` | DirectML 真实模型与压力测试 | 21.09 s | 通过 |
| 4 | `lw.ppocr.openvino.integration` | OpenVINO 真实模型与压力测试 | 28.78 s | 通过 |
| 5 | `lw.ppocr.tensorrt.integration` | TensorRT 真实模型与压力测试 | 3.18 s | 通过 |
| 6 | `lw.ppocr.opencv.integration` | OpenCV DNN 真实模型与压力测试 | 91.21 s | 通过 |
| | **合计** | **6 项** | **144.59 s** | **6/6 通过** |

测试耗时包含模型初始化、预热、一次结果输出、100 次连续调用和 40 次线程调用，
不能作为单次 OCR 延迟使用。单次性能数据见 `docs/performance-report.md`。

## 9. 资源增长结果

| 测试 | 私有内存增长 | 句柄增长 | 阈值 | 结果 |
|---|---:|---:|---|---|
| ABI Stub | 188,416 B | 0 | 16 MiB / 8 | 通过 |
| DirectML | 0 B | 4 | 256 MiB / 32 | 通过 |
| OpenVINO | 16,519,168 B | 5 | 256 MiB / 32 | 通过 |
| TensorRT | 2,998,272 B | 6 | 256 MiB / 32 | 通过 |
| OpenCV DNN | 26,431,488 B | 10 | 256 MiB / 32 | 通过 |

这里的“私有内存增长”是 Windows `PrivateUsage` 在预热基线与压力测试结束之间的
差值。负增长显示为 0。该指标不包含独立的 GPU 显存统计，也不能证明所有 SDK
缓存都会立即归还给操作系统，但可以发现连续增长和明显资源回收回归。

## 10. 测试中发现并处理的问题

首次完整矩阵中，DirectML 在只预热 1 次的基线下，后续动态 shape 和执行计划
缓存使私有内存增长超过 256 MiB，测试失败。此前一轮同样测试能够通过，表现出
基线采集时机不稳定。

处理方式：

1. 将 DirectML 与另外两个加速后端统一预热 8 次。
2. 在缓存稳定后采集资源基线。
3. 单独连续运行 DirectML 测试两轮，均通过，约 21 秒/轮。
4. 再执行完整 6 项矩阵，最终 6/6 通过。

这个修改只调整稳定性测试的基线时机，不改变 DirectML 推理实现或公开 API。

测试日志中的 ONNX Runtime 警告说明部分 shape 相关节点由 CPU 执行，这是 ORT
的正常节点分配提示；本轮没有出现推理失败。OpenCV 5.0 也输出了新图引擎暂不
支持目标选择的警告，但推理和压力测试均成功。

## 11. 版本与发布包验证

| 文件 | FileVersion | ProductVersion |
|---|---|---|
| `lw.PPOCR.dll` | 0.2.0 | 0.2.0 |
| `lw.PPOCR.Runtime.OpenCVDNN.dll` | 0.2.0 | 0.2.0 |
| `lw.PPOCR.Runtime.DirectML.dll` | 0.2.0 | 0.2.0 |
| `lw.PPOCR.Runtime.OpenVINO.dll` | 0.2.0 | 0.2.0 |
| `lw.PPOCR.Runtime.TensorRT.dll` | 0.2.0 | 0.2.0 |
| `lw.PPOCR.Cli.exe` | 0.2.0.0 | `0.2.0+7fde528...` |
| `lw.PPOCR.Demo.exe` | 0.2.0.0 | `0.2.0+7fde528...` |

发布目录的内部 `package-files.sha256` 共验证 60 个文件，全部匹配。

| 发布资产 | 数值 |
|---|---|
| ZIP | `lw.PPOCR.Inference-v0.2.0-win-x64.zip` |
| 文件大小 | 391,692,605 B |
| SHA-256 | `161492d1af08dfabf6e1c7c8178557eb0adc3a87bf36ce0c5a4d4039e2dc6a3d` |

## 12. 结论

lw.PPOCR.Inference v0.2.0 在本报告环境中完成了公共 ABI、生命周期、长循环、
共享实例线程调用、多实例、非法输入、四后端真实模型和发布包完整性验证。
最终完整矩阵 6/6 通过，所有被测进程资源增长均低于设定门槛，没有观察到持续
增长导致的测试失败或程序异常退出。

本轮结果支持将 v0.2.0 作为稳定性增强版本发布。它不等于生产环境无限时长
无泄漏证明；正式部署仍建议结合客户图片集执行数小时或数万次 soak test，
并同时监控进程私有内存、工作集、句柄和 GPU 显存。

## 13. 后续建议

- 为 OpenCV DNN 增加启用 CLS 的独立真实模型用例。
- 增加固定文本、置信度容差和文字框坐标容差的 golden test。
- 增加 1,000 至 10,000 次真实模型 soak test，并按阶段记录内存曲线。
- 增加独立 GPU 显存采样，区分主机私有内存与显存缓存。
- 增加多图片尺寸、超宽文本、高分辨率票证和空白图片测试集。
- 在 CI 中拆分 ABI 快速门槛与具备 SDK/GPU 的真实 Runtime 门槛。
