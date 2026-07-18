# Docker / Docker Compose 部署

项目提供可直接运行的 Dockerfile 和 Docker Compose 配置。默认镜像使用正式发布的
`v1.3.0 linux-x64-opencv` 包；构建镜像时只下载并校验 Release 附件，不会重新编译
OpenCV。镜像已经包含 PP-OCR 模型、OpenCV DNN Runtime、HTTP 服务和测试网页。

当前默认容器面向 Linux x86_64，推理设备为 CPU。Windows 和 macOS 上可以通过
Docker Desktop 运行 Linux 容器，但这不等于 DirectML、WinForms 或 Windows Service
进入了容器。ARM64 容器需要对应架构的正式 Release 包，不能直接运行 x64 镜像。

## 快速启动

需要 Docker Engine 与 Docker Compose v2 插件。在仓库根目录执行：

```bash
cp .env.example .env
# 编辑 .env，为局域网或反向代理部署设置一个足够长的 LW_PPOCR_API_KEY
docker compose up -d --build
docker compose ps
docker compose logs -f http-service
```

PowerShell 可以使用：

```powershell
Copy-Item .env.example .env
docker compose up -d --build
```

启动后访问 `http://127.0.0.1:8787/`。网页包含完整 OCR、只识别、结果区域绘制和耗时
显示。健康检查地址为 `http://127.0.0.1:8787/health`。

停止或更新服务：

```bash
docker compose down
docker compose build --pull
docker compose up -d
```

## 配置

Compose 从根目录的 `.env` 读取以下参数：

| 参数 | 默认值 | 说明 |
|---|---:|---|
| `PPOCR_HTTP_PORT` | `8787` | 宿主机监听端口 |
| `LW_PPOCR_API_KEY` | 空 | HTTP API Key；非本机部署建议设置 |
| `LW_PPOCR_WORKER_THREADS` | `4` | HTTP 工作线程，范围 1-64 |
| `LW_PPOCR_MAX_REQUEST_BYTES` | `20971520` | 单次 HTTP 请求最大字节数 |
| `LW_PPOCR_MAX_IMAGE_PIXELS` | `40000000` | 解码后图片最大像素数 |
| `PPOCR_VERSION` | `1.3.0` | 下载的 Release 版本 |
| `PPOCR_PACKAGE` | `...linux-x64-opencv` | Release 包名，不含 `.tar.gz` |
| `PPOCR_SHA256` | 已固定 | Release 压缩包 SHA-256；构建时强制校验 |

容器内部固定监听 `0.0.0.0:8787`，宿主机端口由 `PPOCR_HTTP_PORT` 控制。默认启用
只读根文件系统、非 root 用户、移除 Linux capabilities，并只给 `/tmp` 提供临时写入空间。

如需使用完整的自定义 `http-service.json`，在 `compose.yaml` 的服务下增加：

```yaml
volumes:
  - ./http-service.json:/config/http-service.json:ro
environment:
  LW_PPOCR_CONFIG_FILE: /config/http-service.json
```

自定义配置中的 `runtime_root`、`model_manifest`、`web_root` 和 `runtime_library` 相对路径
仍以容器内 `/opt/lw-ppocr` 为基准。监听地址、端口、API Key、线程数和请求限制会被
Compose 环境变量覆盖。

## API Key

`.env` 示例：

```dotenv
LW_PPOCR_API_KEY=replace-with-a-long-random-secret
```

启用后，`POST /api/ocr` 和 `POST /api/recognize` 必须携带相同的 `X-API-Key`：

```bash
curl -X POST http://127.0.0.1:8787/api/ocr \
  -H 'Content-Type: application/json' \
  -H 'X-API-Key: replace-with-a-long-random-secret' \
  --data-binary '{"image_base64":"..."}'
```

`GET /health` 不需要 API Key，方便 Docker 健康检查。请不要把真实密钥提交到仓库；
`.env` 已加入 `.gitignore`。面向公网时仍应在前面部署 HTTPS 反向代理、限制来源地址，
不要把 OCR 服务端口直接暴露到互联网。

## English quick start

The supplied Compose stack builds an image from the verified v1.3.0 Linux x64
OpenCV DNN Release package. It includes the models, native runtime, HTTP server,
and browser test page.

```bash
cp .env.example .env
docker compose up -d --build
docker compose logs -f http-service
```

Open `http://127.0.0.1:8787/`. Set `LW_PPOCR_API_KEY` in `.env` before LAN or
reverse-proxy deployment and send it in the `X-API-Key` header. The default image
is Linux x86_64 CPU-only; ARM64 requires a matching ARM64 Release package.

