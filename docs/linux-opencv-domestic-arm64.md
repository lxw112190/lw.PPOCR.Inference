# 国产 Linux ARM64 OpenCV DNN HTTP 服务 CI 产物

该文档用于 Anolis OS 8.10、OpenCloudOS 9.4 等 RPM 系国产 Linux ARM64
矩阵构建产物。每个 GitHub Actions Artifact 都是在对应发行版官方 AArch64
用户态中原生编译、打包并重新执行真实 OCR 测试的独立产物，不是交叉编译结果。

压缩包内的 `BUILD-ENVIRONMENT.txt` 记录实际发行版、glibc、GCC 和架构。不同
发行版 Artifact 中的压缩包文件名相同，请保留外层 Artifact 名称，不要混放或
把其中的 `.so` 相互替换。

## 当前验证范围

| Artifact 后缀 | 构建用户态 | 状态 |
|---|---|---|
| `anolis810` | Anolis OS 8.10 AArch64 | CI 构建与运行验证 |
| `opencloudos94` | OpenCloudOS 9.4 AArch64 | CI 构建与运行验证 |

CI 通过表示服务能在官方容器用户态内完成模型加载、完整 OCR、单张/批量只识别、
HTTP API、API Key 和打包后重启测试。只有对应实体机或客户环境也通过验证后，才
应标记为正式支持。

## 快速验证

先确认机器输出 `aarch64`：

```bash
uname -m
```

将 Artifact ZIP 中的 `.tar.gz` 和 `.tar.gz.sha256` 放到目标机器：

```bash
sha256sum -c lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz.sha256
tar -xzf lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv.tar.gz
cd lw.PPOCR.Inference-v1.4.0-preview.1-linux-arm64-opencv

cat BUILD-ENVIRONMENT.txt
sudo ./install-deps-rpm.sh
./verify-linux-package.sh
./run-http-service.sh
```

浏览器访问 `http://127.0.0.1:8787/`。`verify-linux-package.sh` 会检查包内
SHA-256、AArch64 ELF 依赖、健康接口，并执行一次真实 OCR。运行验证前请确保
8787 端口没有被其他程序占用。

## HTTP 服务配置

默认配置位于 `http-service.json`，只监听 `127.0.0.1:8787`。局域网部署时应改为：

```json
{
  "listen_host": "0.0.0.0",
  "api_key": "replace-with-a-long-random-secret"
}
```

启用后，`POST /api/ocr` 与 `POST /api/recognize` 必须携带相同的
`X-API-Key` 请求头；`GET /health` 保持免认证。不要把 8787 端口直接暴露到公网。

确认前台运行和 API 调用正常后，可以安装 systemd 服务：

```bash
sudo ./install-systemd.sh
systemctl status lw-ppocr-http.service
journalctl -u lw-ppocr-http.service -f
```

## English summary

These CI artifacts are built and tested natively inside the official Anolis OS
8.10 or OpenCloudOS 9.4 AArch64 userspace. Keep artifacts from different
distributions separate, inspect `BUILD-ENVIRONMENT.txt`, install dependencies
with `sudo ./install-deps-rpm.sh`, and run `./verify-linux-package.sh` before
deployment. A passing container CI run is compatibility evidence, but formal
support still requires validation on the corresponding physical or customer
environment.

