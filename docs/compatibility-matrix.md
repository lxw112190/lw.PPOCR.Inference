# Compatibility Matrix

> 本矩阵记录已验证的后端、OS、GPU、驱动和 SDK 组合。未列出的组合不保证可用。
> 标记为 "Planned / 规划中" 的为预期支持但尚未实测的组合。
>
> This matrix documents verified backend, OS, GPU, driver, and SDK combinations.

---

## OpenCV DNN

| OS | CPU Arch | OpenCV | 验证版本 | 状态 |
|---|---|---|---|---|
| Windows 10 22H2 (19045) | x64 | 5.0.0 | v0.2.0 | ✅ |
| Windows 11 | x64 | 5.0.0 | — | Planned |
| Windows Server 2022 | x64 | 5.0.0 | — | Planned |

**已知限制:**
- OpenCV 5.0 新图引擎在初始化时输出 `setPreferableTarget Targets are not supported` warning，不影响推理结果。
- 不支持 FP16 / INT8（DNN 后端无量化加速路径）。

---

## ONNX Runtime DirectML

| OS | GPU 厂商 | GPU 示例 | 驱动模型 | ORT | DirectML | 验证版本 | 状态 |
|---|---|---|---|---|---|---|---|
| Windows 10 22H2 | NVIDIA | RTX 4060 Laptop | WDDM 3.1 | 1.23.0 | 1.15.4 | v0.2.0 | ✅ |
| Windows 10 22H2 | AMD | Radeon Graphics (iGPU) | WDDM 3.1 | 1.23.0 | 1.15.4 | v0.2.0 | ✅ |
| Windows 10 22H2 | Intel | — | WDDM 2.0+ | 1.23.0 | 1.15.4 | — | Planned |
| Windows 11 | NVIDIA | — | WDDM 3.x | 1.23.0 | 1.15.4 | — | Planned |

**已知限制:**
- CPU fallback (`use_gpu=0`) 可用，但性能远低于 OpenVINO CPU。
- ORT 可能将部分 shape 相关节点分配至 CPU，输出 `node placement` warning，属正常行为。
- 引擎级 `run_mutex` 串行化同实例调用，高并发场景需使用多实例。
- INT8 未启用。

---

## ONNX Runtime CPU / CUDA（Linux）

| OS | 设备 | ONNX Runtime | CUDA | 验证版本 | 状态 |
|---|---|---|---|---|---|
| Windows 10 22H2 | CPU | 1.26.0 GPU 包的 CPU EP | 不使用 | v1.3.0-preview.1 | ✅ 本地源码回归 |
| Ubuntu 20.04 x64 | CPU | 1.26.0 CPU 官方包 | 不使用 | v1.3.0-preview.1 | CI 待运行 |
| Ubuntu 20.04 x64 | NVIDIA GPU | 1.26.0 GPU 官方包 | CUDA 12 / 13 对应包 | v1.3.0 | 实机待验证 |

**约束：**

- CPU 与 GPU 动态库必须来自同一 ONNX Runtime 版本；替换时复制完整 `libonnxruntime*.so*` 集合。
- CUDA 12 与 CUDA 13 官方包不可混用；GPU 还要求兼容的 NVIDIA 驱动、CUDA、cuDNN 和 zlib。
- `device=cpu` 强制 CPU，`device=cuda` 强制 CUDA，`device=auto` 允许 CUDA 初始化失败后回退 CPU。

---

## OpenVINO

| OS | CPU | OpenVINO | 验证版本 | 状态 |
|---|---|---|---|---|
| Windows 10 22H2 | AMD Ryzen 7 7735H | 2026.2.0 (21903) | v0.2.0 | ✅ |
| Windows 11 | — | 2026.2.0 | — | Planned |

**已知限制:**
- **禁止 GPU。** OpenCL mapping failure (`clEnqueueMapBuffer: CL_INVALID_VALUE`) 已复现，稳定版本中 GPU 选择会被拒绝。
- Recognition concurrency 上限为 8。
- 曾出现 CPU GroupConvolution `can't alloc`，需继续做长循环验证。

---

## TensorRT

| OS | GPU | CUDA | TensorRT | 驱动 | 验证版本 | 状态 |
|---|---|---|---|---|---|---|
| Windows 10 22H2 | RTX 4060 Laptop (Ada) | 12.6.20 | 10.16.1.11 | 596.36 | v0.2.0 | ✅ |
| Windows 11 | RTX 40 series | 12.6+ | 10.x | 596+ | — | Planned |
| Windows 10/11 | RTX 30 series (Ampere) | 12.x | 10.x | — | — | Planned |

**已知限制:**
- **Engine 不可跨机器分发。** 与 GPU 架构、TensorRT、CUDA 和驱动版本绑定。
- 仅使用预生成 FP16 engine；不在程序初始化时调用 `trtexec`。
- INT8 路径已移除。
- Engine 优化 profile 指定了最大 batch/shape；超出需重新生成 engine。

---

## .NET Binding

| .NET | OS | 架构 | 验证版本 | 状态 |
|---|---|---|---|---|
| .NET 8.0 | Windows 10 22H2 | x64 | v0.2.0 | ✅ |
| .NET Framework 4.7.2 | Windows 10/11 | x64 | v1.1.0 | ✅ (WinForms Demo) |
| .NET 8.0 | Windows 11 | x64 | — | Planned |
| .NET 9.0 | Windows 10/11 | x64 | — | Planned |

**已知限制:**
- 绑定库支持 `net472` 与 `net8.0`；WinForms Demo 支持 `net472` 与 `net8.0-windows`，部署包默认携带 `net472` 版本。
- `OcrEngine.Run()` 接受 `byte[]`，不直接支持 `Bitmap`（可通过适配层转换）。

---

## C ABI

| 语言 | 调用方式 | 验证版本 | 状态 |
|---|---|---|---|
| C | `LoadLibrary` + `GetProcAddress` / 直接链接 | v0.5.0 | ✅ (example) |
| C++ | 直接链接 + `extern "C"` 头文件 | v0.1.0 | ✅ |
| C# | P/Invoke | v0.1.0 | ✅ |
| Python | ctypes | v0.5.0 | ✅ (example) |
| C / C# / Python | `recognize` / `recognize_batch` | v1.1.0 | ✅ |
| Delphi | — | — | Planned |
| Go | cgo / syscall | — | Planned |
| Rust | `extern "C"` / `libloading` | — | Planned |
| Java | JNI / JNA | — | Planned |

---

## 最低系统要求 (Minimum System Requirements)

| 要求 | 规格 |
|---|---|
| OS | Windows 10 x64 (build 19041+) 或 Windows 11 x64 |
| CPU | x86-64 with AVX2 (OpenCV DNN / OpenVINO) |
| 内存 | ≥ 4 GB（模型加载 + 推理；实际取决于图片尺寸和并发数） |
| GPU (DirectML) | DirectX 12 capable, WDDM 2.0+ |
| GPU (TensorRT) | NVIDIA CUDA compute capability ≥ 7.0, 驱动 ≥ 536 |
| .NET Runtime | .NET 8 Desktop Runtime（CLI 和 Demo 需要） |

---

## 不支持的组合 (Unsupported)

| 组合 | 原因 |
|---|---|
| 32-bit (x86) | 项目仅编译 x64；32-bit 未测试 |
| Windows 7 / 8 / 8.1 | 部分 SDK（DirectML、最新 OpenCV/ORT）不再支持 |
| Linux / macOS | 项目当前仅面向 Windows；跨平台为远期目标 |
| ARM64 Windows | 未测试；部分 SDK 无 ARM64 原生包 |
| OpenVINO GPU | 已复现 OpenCL mapping failure，稳定版本禁用 |
| TensorRT INT8 | 无校准数据和完整准确率验证 |
