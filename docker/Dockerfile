# syntax=docker/dockerfile:1
ARG BASE_IMAGE=ubuntu:20.04
FROM ${BASE_IMAGE}

ARG PPOCR_VERSION=1.3.0
ARG PPOCR_PACKAGE=lw.PPOCR.Inference-v1.3.0-linux-x64-opencv
ARG PPOCR_SHA256=4ed127d541d0f9077a78261513801650ecd9524d69ef377c200898184398a208

LABEL org.opencontainers.image.title="lw.PPOCR.Inference HTTP Service" \
      org.opencontainers.image.description="PP-OCR HTTP service with the OpenCV DNN Runtime" \
      org.opencontainers.image.source="https://github.com/lxw112190/lw.PPOCR.Inference" \
      org.opencontainers.image.version="${PPOCR_VERSION}" \
      org.opencontainers.image.authors="天天代码码天天 <QQ 819069052>"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        jq \
        libgomp1 \
        libjpeg8 \
        libnuma1 \
        libpng16-16 \
        libtiff5 \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

RUN curl --fail --location --retry 5 --retry-delay 5 \
        "https://github.com/lxw112190/lw.PPOCR.Inference/releases/download/v${PPOCR_VERSION}/${PPOCR_PACKAGE}.tar.gz" \
        --output /tmp/lw-ppocr.tar.gz \
    && echo "${PPOCR_SHA256}  /tmp/lw-ppocr.tar.gz" | sha256sum --check --strict - \
    && mkdir -p /opt/lw-ppocr \
    && tar -xzf /tmp/lw-ppocr.tar.gz --strip-components=1 -C /opt/lw-ppocr \
    && test -x /opt/lw-ppocr/lw-ppocr-http-service \
    && test -f /opt/lw-ppocr/models/ppocrv6-tiny/model.json \
    && test -f /opt/lw-ppocr/www/index.html \
    && rm /tmp/lw-ppocr.tar.gz

COPY docker/docker-entrypoint.sh /usr/local/bin/lw-ppocr-entrypoint
COPY docker/configure.jq /usr/local/share/lw-ppocr/configure.jq
RUN chmod 0755 /usr/local/bin/lw-ppocr-entrypoint

ENV LW_PPOCR_LISTEN_HOST=0.0.0.0 \
    LW_PPOCR_PORT=8787 \
    LW_PPOCR_WORKER_THREADS=4 \
    LW_PPOCR_MAX_REQUEST_BYTES=20971520 \
    LW_PPOCR_MAX_IMAGE_PIXELS=40000000 \
    LW_PPOCR_CONFIG_FILE=/opt/lw-ppocr/http-service.json \
    LD_LIBRARY_PATH=/opt/lw-ppocr:/opt/lw-ppocr/runtimes/linux-x64/opencv:/opt/lw-ppocr/runtimes/linux-x64/onnxruntime:/opt/lw-ppocr/runtimes/linux-x64/openvino

WORKDIR /opt/lw-ppocr
USER 65532:65532
EXPOSE 8787

HEALTHCHECK --interval=30s --timeout=5s --start-period=30s --retries=3 \
  CMD curl --fail --silent --show-error "http://127.0.0.1:${LW_PPOCR_PORT}/health" >/dev/null || exit 1

ENTRYPOINT ["/usr/local/bin/lw-ppocr-entrypoint"]
