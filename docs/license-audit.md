# License Audit

> 审计范围：lw.PPOCR.Inference 直接或间接依赖的所有第三方组件。
> 审计日期：v0.8.0 发布前。 最终更新：2026-07-16。
>
> Audit scope: all third-party components that lw.PPOCR.Inference directly or
> indirectly depends on.  Last updated: see git log.

---

## 1. Direct Dependencies (bundled or linked)

### Clipper 6.4.2

| 项目 | 内容 |
|---|---|
| 作者 / Author | Angus Johnson |
| 许可证 / License | [Boost Software License 1.0](https://www.boost.org/LICENSE_1_0.txt) |
| 位置 / Location | `third_party/clipper/` |
| 使用方式 | 源码级引入，编译入各 Runtime DLL |
| 修改 | 无；使用上游 6.4.2 |
| 与 MIT 兼容性 | ✅ 兼容 — BSL-1.0 允许再分发，仅要求保留版权声明 |
| 分发要求 | 保留 LICENSE.txt；已安装在 `licenses/clipper-BSL-1.0.txt` |
| 状态 | ✅ 合规 |

### OpenCV 5.0.0

| 项目 | 内容 |
|---|---|
| 许可证 / License | [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) |
| 使用方式 | 动态链接 (`opencv_world500.dll`) |
| 与 MIT 兼容性 | ✅ 兼容 — Apache-2.0 是宽松许可证 |
| 分发要求 | 保留 NOTICE 和 LICENSE；发布包随附 `opencv-APACHE-2.0.txt` |
| 状态 | ✅ 许可证文本已随附 |

### ONNX Runtime 1.23.0 / 1.26.0

| 项目 | 内容 |
|---|---|
| 许可证 / License | [MIT License](https://github.com/microsoft/onnxruntime/blob/main/LICENSE) |
| 使用方式 | 动态链接（Windows DLL；Linux `libonnxruntime*.so*`） |
| 与 MIT 兼容性 | ✅ 同许可证 |
| 分发要求 | 保留 LICENSE 和第三方声明；发布包均已随附 |
| 状态 | ✅ 通用声明已随附；Linux 1.26.0 构建还会从官方包安装该包对应的 LICENSE 与 ThirdPartyNotices |

### DirectML 1.15.4

| 项目 | 内容 |
|---|---|
| 许可证 / License | [MIT License](https://github.com/microsoft/DirectML/blob/master/LICENSE) |
| 使用方式 | 动态链接 (`DirectML.dll`) |
| 与 MIT 兼容性 | ✅ 同许可证 |
| 分发要求 | 保留 LICENSE |
| 状态 | ✅ `DirectML-MIT.txt` 已随附 |

### OpenVINO 2026.2.0

| 项目 | 内容 |
|---|---|
| 许可证 / License | [Apache License 2.0](https://github.com/openvinotoolkit/openvino/blob/master/LICENSE) |
| 使用方式 | 动态链接 (`openvino.dll`, `openvino_onnx_frontend.dll`, `openvino_intel_cpu_plugin.dll`, `tbb12.dll`, `tbbmalloc.dll`) |
| 与 MIT 兼容性 | ✅ 兼容 |
| 分发要求 | 保留 LICENSE 和分发材料；发布包已随附 |
| 状态 | ✅ `openvino-APACHE-2.0.txt` 与 `openvino-EULA.txt` 已随附 |

### TensorRT 10.16.1.11

| 项目 | 内容 |
|---|---|
| 许可证 / License | [NVIDIA TensorRT License Agreement](https://docs.nvidia.com/deeplearning/tensorrt/sla/) (EULA) |
| 使用方式 | 动态链接 (`nvinfer_10.dll`, `nvinfer_plugin_10.dll`) |
| 与 MIT 兼容性 | ⚠️ 见下文分析 |
| 分发要求 | 见下文 |
| 状态 | ⚠️ 上游许可证引用和第三方声明已随附；使用仍受 NVIDIA 协议约束 |

### CUDA Runtime (cudart64_12.dll)

| 项目 | 内容 |
|---|---|
| 许可证 / License | [NVIDIA CUDA Toolkit EULA](https://docs.nvidia.com/cuda/eula/) |
| 使用方式 | 动态链接 (`cudart64_12.dll`) |
| 与 MIT 兼容性 | ⚠️ 见下文分析 |
| 分发要求 | 见下文 |
| 状态 | ⚠️ `CUDA-EULA.txt` 已随附；使用仍受 NVIDIA 协议约束 |

---

## 2. NVIDIA 组件再分发分析

### TensorRT EULA 关键条款

NVIDIA TensorRT EULA 通常允许：
- 将 TensorRT DLL 作为应用程序的一部分再分发（"distribute the
  redistributable components of the TensorRT SDK"）
- 前提是最终用户接受 EULA 条款
- 不能将 TensorRT 作为独立 SDK 分发
- 必须仅用于运行使用 TensorRT 构建的应用程序

**当前项目状态**: `dist/lw.PPOCR.Inference-win-x64/runtimes/win-x64/tensorrt/` 中
包含 `nvinfer_10.dll` 和 `nvinfer_plugin_10.dll`。根据 NVIDIA EULA，**作为
Runtime 依赖随应用程序分发应属合规**，但需在发布包中随附 TensorRT EULA 文本和
归属声明。

### CUDA Runtime EULA 关键条款

NVIDIA CUDA EULA 对 `cudart64_12.dll` 的再分发有类似规定。作为 CUDA 应用程序的
运行时依赖分发通常是允许的。

### 建议措施

1. **在 `licenses/` 目录中随附：**
   - TensorRT 上游许可证引用及第三方声明
   - CUDA Toolkit EULA
2. **在 README 和 Release Notes 中明确声明：**
   - TensorRT 和 CUDA 运行时组件属于 NVIDIA Corporation
   - 最终用户使用这些组件须遵守 NVIDIA EULA
   - 如果用户不接受 NVIDIA EULA，应使用 OpenCV DNN、DirectML 或 OpenVINO 后端
3. **考虑提供不含 NVIDIA 组件的分发包：**
   - 已有 `-Split` 打包方案，用户可选择不下载 TensorRT 包
   - TensorRT 包应单独标注 EULA 要求

### 结论

在采取上述措施后，TensorRT/CUDA 组件随 lw.PPOCR.Inference 分发**应为合规**。
但这**不是法律意见**；建议在实际发布前由法律专业人士审查。

---

## 3. HTTP Service Dependencies

| 组件 | 版本 | 许可证 | 使用与分发状态 |
|---|---:|---|---|
| cpp-httplib | 0.49.0 | MIT | ✅ 单头文件编译进 HTTP Service；许可证已随附 |
| nlohmann/json | 3.12.0 | MIT | ✅ 单头文件编译进 HTTP Service；许可证已随附 |
| Windows Imaging Component | Windows 系统组件 | Windows SDK / OS | ✅ 仅动态调用系统组件，不随包复制 |

固定头文件 SHA-256：

- cpp-httplib `6920f3d19a3a239bd03e1a0ce337720083801c62f273b6d5d17a7ef31e567e2e`
- nlohmann/json `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`

---

## 4. Build-time Dependencies (not redistributed)

| 组件 | 许可证 | 分发 |
|---|---|---|
| Visual Studio 2022 / MSVC | Visual Studio License | ❌ 不分发 |
| CMake 3.31 | BSD-3-Clause | ❌ 不分发 |
| .NET SDK 8.0 | MIT | ❌ 不分发 |

这些仅在开发/构建时使用，不包含在发布包中，无需额外审计。

---

## 5. 模型文件

### PP-OCRv6 tiny ONNX models

| 项目 | 内容 |
|---|---|
| 来源 | PaddleOCR / PP-OCRv6 官方模型 |
| 原始框架 | PaddlePaddle |
| 转换 | 由用户自行转换为 ONNX 格式 |
| 原始模型许可证 | [Apache License 2.0](https://github.com/PaddlePaddle/PaddleOCR/blob/main/LICENSE) |
| 状态 | ✅ 原始模型为 Apache-2.0；ONNX 转换物的许可证状态通常跟随原始模型 |

### ppocr_keys.txt

| 项目 | 内容 |
|---|---|
| 来源 | PaddleOCR 官方字典 |
| 许可证 | 与 PaddleOCR 模型相同 (Apache-2.0) |
| 状态 | ✅ |

---

## 6. Summary / 总结

| 组件 | 许可证 | 合规状态 | 操作 |
|---|---|---|---|
| lw.PPOCR.Inference | MIT | ✅ | — |
| Clipper 6.4.2 | BSL-1.0 | ✅ | 已随附 LICENSE |
| OpenCV 5.0.0 | Apache-2.0 | ✅ | LICENSE 已随附 |
| ONNX Runtime 1.23.0 / 1.26.0 | MIT | ✅ | LICENSE 与第三方声明已随附 |
| DirectML 1.15.4 | MIT | ✅ | LICENSE 已随附 |
| OpenVINO 2026.2.0 | Apache-2.0 | ✅ | LICENSE 与分发材料已随附 |
| TensorRT 10.16.1.11 | NVIDIA 协议 | ⚠️ | 上游许可证引用 + 第三方声明 + README 声明 |
| CUDA 12.6 Runtime | NVIDIA EULA | ⚠️ | EULA + README 声明 |
| PP-OCRv6 tiny models | Apache-2.0 | ✅ | — |
| cpp-httplib 0.49.0 | MIT | ✅ | LICENSE 已随附 |
| nlohmann/json 3.12.0 | MIT | ✅ | LICENSE 已随附 |

### Action Items (recommended before v1.0.0)

- [x] 在 `licenses/` 目录中增加第三方许可证、引用和声明材料
- [x] 在 README 和 Release Notes 中增加 NVIDIA 组件协议声明
- [ ] 考虑在 TensorRT 下载页面增加 EULA 接受提示
- [ ] 如条件许可，咨询法律专业人士对 NVIDIA EULA 合规性进行审查
