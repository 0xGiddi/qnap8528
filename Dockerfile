FROM debian:bookworm AS builder

# 接收宿主机内核的 GCC 版本（由 build.sh 传入）
ARG GCC_VERSION

# 替换为阿里云 Debian 镜像源
RUN sed -i 's|deb.debian.org|mirrors.aliyun.com|g' /etc/apt/sources.list.d/debian.sources

# 安装编译工具
# 指定 GCC 版本时：阿里云镜像 → snapshot.debian.org → 默认版本，逐级兜底
RUN apt-get update && \
    if [ -n "$GCC_VERSION" ]; then \
        echo "📦 尝试安装指定版本 GCC: $GCC_VERSION" && \
        (apt-get install -y --no-install-recommends gcc=$GCC_VERSION || \
         (echo "⚠️ 阿里云镜像无此版本，尝试从 snapshot.debian.org 获取..." && \
          echo "deb [check-valid-until=no] https://snapshot.debian.org/archive/debian/20260701T000000Z/ bookworm main" \
              > /etc/apt/sources.list.d/snapshot.list && \
          apt-get -o Acquire::Check-Valid-Until=false update && \
          apt-get install -y --no-install-recommends gcc=$GCC_VERSION) || \
         (echo "⚠️ 指定版本均不可用，使用默认版本" && \
          apt-get install -y --no-install-recommends gcc)); \
    else \
        apt-get install -y --no-install-recommends gcc; \
    fi && \
    apt-get install -y --no-install-recommends \
        make \
        build-essential \
        libncurses-dev \
        libelf1 && \
    rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /driver
