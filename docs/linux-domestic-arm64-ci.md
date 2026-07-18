# 国产 Linux ARM64 CI 与兼容范围

本文说明 `lw.PPOCR.Inference` OpenCV DNN HTTP 服务在国产 Linux 服务器操作系统上的自动化验证范围。当前阶段聚焦 **AArch64/ARM64 + CPU**；x86_64 已由现有 Ubuntu Linux CI 覆盖，LoongArch64、MIPS64、SW64 与 ARM32 不是 ARM64 二进制兼容架构，需要独立工具链和实体机验证。

## 常见国产服务器操作系统

| 类型 | 常见系统 | 当前处理方式 |
|---|---|---|
| 公开社区发行版 | openEuler、Anolis OS（龙蜥）、OpenCloudOS | 使用官方 ARM64 容器做原生 CI |
| 商业信创发行版 | 银河麒麟高级服务器操作系统 V10、统信 UOS 服务器版 V20 | 使用厂商官方镜像或授权实体机验证 |
| 行业/云厂商发行版 | 中移动 BC-Linux、天翼云 CTyunOS、TencentOS Server、Alibaba Cloud Linux | 按客户实际版本增加实体机或自托管 Runner |
| 桌面社区发行版 | openKylin、deepin | 当前 HTTP 服务发布不作为主要服务器基线 |

“国产系统”不是一个统一 ABI。即使都运行在 ARM64 上，不同版本的 glibc、libstdc++、系统图像编解码库和包管理器仍然可能不同。因此 CI 必须在目标发行版用户态中真正启动 HTTP 服务并执行 OCR，不能只检查能否交叉编译。

## 自动 CI 第一批

| 系统 | 基线 | 镜像来源 | 验证内容 |
|---|---|---|---|
| openEuler | 22.03 LTS-SP1 AArch64 | openEuler 官方容器归档 | 已通过 CI 与 ARM64 实体机验证 |
| Anolis OS | 8.10 AArch64 | `registry.openanolis.cn/openanolis/anolisos:8.10` | ✅ CI 已验证（编译、测试、打包后真实 OCR） |
| OpenCloudOS | 9.4 AArch64 | `opencloudos/opencloudos9-minimal:9.4-v20260424` | ✅ CI 已验证（编译、测试、打包后真实 OCR） |

工作流：

```text
.github/workflows/linux-arm64-opencv-openeuler.yml
.github/workflows/linux-arm64-domestic.yml
```

每个发行版独立完成：

1. 拉取厂商官方 ARM64 镜像并确认 `/etc/os-release`、`uname -m`、glibc 和 GCC。
2. 使用通用 `-march=armv8-a` 编译精简 OpenCV 5.0。
3. 使用发行版专属缓存保存 OpenCV 安装目录。
4. 编译 Loader、OpenCV Runtime、HTTP 服务和测试程序。
5. 执行 ABI、golden、真实 OCR、并发、HTTP 与批量只识别测试。
6. 生成完整部署包并从打包目录重新启动服务。
7. 使用 `readelf` 和 `ldd` 验证 AArch64 架构及动态库完整性。

Anolis OS 8.10 默认 GCC 8，工程会在该编译器上为 C++17 filesystem 自动链接 `stdc++fs`。CI 显式安装并使用 Anolis 8.10 的 Python 3.11 兼容包，以满足模型校验和 HTTP 冒烟脚本的 Python 3.10+ 语法要求。不同发行版的 OpenCV 缓存互不复用，避免把某一系统的 glibc 或 libstdc++ 依赖带入另一系统。

矩阵中的每个发行版都会上传独立 Artifact：

```text
lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv-anolis810
lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv-opencloudos94
```

外层 Artifact 名称区分构建用户态，压缩包内的 `BUILD-ENVIRONMENT.txt` 记录发行版、glibc、GCC 与架构。压缩包文件名为保持 API/发布命名一致而相同，不应把不同 Artifact 解压到同一目录或混用其中的 `.so`。矩阵包使用 [国产 Linux ARM64 部署说明](linux-opencv-domestic-arm64.md)，并保留 `install-deps-openeuler.sh` 兼容入口；Anolis/OpenCloudOS 推荐运行通用的 `install-deps-rpm.sh`。

## 银河麒麟与统信 UOS

银河麒麟 V10 和统信 UOS V20 都提供 ARM64 产品与 OCI/Docker 镜像，但实际项目经常使用不同 SP、补丁、CPU 厂商适配版和授权仓库。正式兼容结论应绑定到完整版本，例如：

```text
Kylin-Server-V10-SP3-2403 ARM64
UnionTech OS Server V20 1050e ARM64
```

仓库不使用个人维护的第三方“麒麟/UOS”镜像作为正式 CI 依据。获得厂商官方容器镜像后，可以在 GitHub Actions 中增加私有 Registry 凭据；获得实体机后，可以注册带发行版标签的 ARM64 self-hosted Runner。

在目标机器或官方容器中安装 GCC、CMake 3.24+、Ninja、Python 3、jsonschema、OpenCV 编译依赖后，可复用同一脚本：

```bash
export CI_DISTRO_ID=kylinv10-sp3
bash scripts/ci-linux-arm64-opencv.sh opencv
bash scripts/ci-linux-arm64-opencv.sh project
```

统信 UOS 示例：

```bash
export CI_DISTRO_ID=uos-v20
bash scripts/ci-linux-arm64-opencv.sh opencv
bash scripts/ci-linux-arm64-opencv.sh project
```

`CI_DISTRO_ID` 同时隔离 OpenCV 缓存和 CMake 构建目录。脚本拒绝在非 `aarch64` 环境运行，避免错误地产生 x86_64 包。

## 发布策略

当前正式候选附件仍以已经完成实体机验证的 openEuler ARM64 包为主。Anolis OS 和 OpenCloudOS 已记录为“CI 验证”；只有对应实体机或客户环境也通过 `verify-linux-package.sh`，再提升为“正式支持”。

如果后续希望只发布一个通用 ARM64 包，应选择可满足项目编译要求且 glibc 足够旧的基线构建，然后把同一个压缩包放入所有目标系统做运行测试。不要把分别在多个发行版编译出的同名 `.so` 混合到一个包中。

## English summary

The public ARM64 CI matrix covers openEuler 22.03 LTS-SP1, Anolis OS 8.10, and OpenCloudOS 9.4 using vendor-provided images. Each distribution builds OpenCV and the project natively, creates a package, and reruns real OCR and HTTP tests from that package. Kylin V10 and UnionTech OS Server V20 require an official vendor image, a licensed machine, or a labeled self-hosted runner; third-party repackaged images are not accepted as release evidence. LoongArch64, SW64, MIPS64, ARM32, and x86_64 require separate binaries and are not covered by the ARM64 package.
