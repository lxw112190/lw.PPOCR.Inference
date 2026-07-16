# Roadmap

> 已完成里程碑见下方 [Completed](#completed) 节。本节聚焦当前至 v1.0.0 的路线。

## v0.3.0 (已完成) — Schema 冻结、Golden 正确性测试

- [x] 模型清单 JSON Schema v1 冻结：添加 `$id`、字段描述、兼容性承诺
- [x] Golden 正确性测试框架：文字、包围框坐标（±2 px）、置信度（±0.02）逐区域比对
- [x] `LW_PPOCR_RECORD_GOLDEN=1` 录制模式
- [x] 版本号同步至 0.3.0（11 文件）
- 详见 [docs/releases/v0.3.0.md](releases/v0.3.0.md)

## v0.5.0 (已完成) — C/Python 示例、后端拆包、CI

- [x] C 示例：`examples/c/main.c` + `CMakeLists.txt`，LoadLibrary 动态加载
- [x] Python 示例：`examples/python/ppocr.py` + `pyproject.toml`
- [x] 后端拆包：`-Split` / `-Runtime` 参数
- [x] GitHub Actions CI：VS 构建 + ABI + golden

## v0.8.0 (已完成) — ABI 候选冻结、兼容矩阵、许可证审计

- [x] `docs/abi-freeze-candidate.md`：冻结范围和非冻结范围
- [x] `docs/compatibility-matrix.md`：四后端兼容矩阵和最低要求
- [x] `docs/license-audit.md`：第三方许可证完整审计

## v0.9.0-rc.1 (已完成) — 发布候选

- [x] 冻结全部公共接口
- [x] Release Notes 和 RC 验证清单

## v1.0.0 (已完成) — ABI 正式冻结

- [x] `LW_PPOCR_API_VERSION` 永久锁定为 `1`
- [x] `docs/abi-v1-stability.md`：长期支持承诺（安全修复 ≥2 年）
- [x] 向后兼容承诺：struct 只追加、enum 只追加、函数不删
- [x] Schema v1 永久固定

## v1.1.0 (已完成) — 只识别接口与本地服务体验

- [x] C ABI 追加单张/批量只识别接口，保持 API v1 向后兼容
- [x] OpenCV DNN、DirectML、OpenVINO、TensorRT 四后端实现只识别
- [x] C、Python、.NET 绑定与示例覆盖新接口
- [x] WinForms Demo 支持鼠标框选文字区域并立即识别
- [x] HTTP API、测试网页、Windows 服务安装脚本进入部署包

---

## Completed

### Phase 1–2: Contract & OpenCV DNN (v0.1.0)

- [x] 公共 C ABI 定型，UTF-8、生命周期、错误传播验证通过
- [x] 模型清单 Schema 初版
- [x] OpenCV DNN Runtime 完成真实模型端到端测试
- [x] 统一 Loader + Stub Runtime 测试通过

### Phase 3: GPU Runtimes (v0.1.0–v0.2.0)

- [x] DirectML：独立 ORT/DML 依赖、GPU 选择、CLS、CPU fallback
- [x] OpenVINO：CPU-only 稳定版、共享 compiled model、concurrency≤8、GPU 路径禁用
- [x] TensorRT：FP16 engine 直读、共享 ICudaEngine、独立 context/stream/buffer
- [x] 四后端集成测试：真实模型、区域数校验、压力测试（100 次顺序 + 4 线程×10 次）

### Phase 4: Bindings & Distribution (v0.1.0–v0.2.0)

- [x] .NET SafeHandle binding + CLI（UnifiedCli）+ WinForms Demo（UnifiedDemo）
- [x] Windows x64 统一发布包 + `package-files.sha256`

### Phase 5: Stability & Golden (v0.2.0–v0.3.0)

- [x] ABI 稳定性：1,000 生命周期 + 10,000 连续调用 + 8 线程并发 + 多实例
- [x] 私有内存/句柄增长回归检查（≤256 MiB / ≤32 handles）
- [x] 性能报告：四后端 P50/P95/均值/wall time（500×500 图片）
- [x] Schema v1 冻结、Golden 正确性测试
