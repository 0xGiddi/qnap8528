#!/bin/bash
# qnap8528 开机自检脚本
# 检查 qnap8528 内核模块是否已加载，未加载则自动编译安装并重启 coolcontrol
# 可通过飞牛任务计划（开机触发）直接调用，无需安装为 systemd 服务

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG_TAG="qnap8528-boot-check"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1"
    logger -t "$LOG_TAG" "$1"
}

# 检查 qnap8528 模块是否已加载
# 通过 sysfs 检测：内核加载模块后会创建 /sys/module/<模块名> 目录
if [ -d /sys/module/qnap8528 ]; then
    log "✅ qnap8528 模块已加载，无需处理"
    exit 0
fi

log "⚠️ qnap8528 模块未加载，开始自动编译安装..."

# 等待 Docker 服务就绪（最多等待 60 秒）
for i in $(seq 1 60); do
    if systemctl is-active --quiet docker; then
        break
    fi
    log "等待 Docker 服务启动... ($i/60)"
    sleep 1
done

if ! systemctl is-active --quiet docker; then
    log "❌ Docker 服务未就绪，退出"
    exit 1
fi

# 执行编译安装
# 首次使用已有镜像（可能 GCC 版本不对），失败后强制重建镜像重试
cd "$SCRIPT_DIR"
bash ./build.sh --auto

if [ $? -ne 0 ]; then
    log "⚠️ 首次编译失败，尝试重建镜像后重试..."
    bash ./build.sh --auto --rebuild
    if [ $? -ne 0 ]; then
        log "❌ qnap8528 编译安装失败"
        exit 1
    fi
fi

log "✅ qnap8528 编译安装完成，执行后续操作..."

# 编译成功后重启 coolcontrol 容器
if docker ps -a --format '{{.Names}}' | grep -q "^coolcontrol$"; then
    log "🔄 重启 coolcontrol 容器..."
    docker restart coolcontrol
    if [ $? -eq 0 ]; then
        log "✅ coolcontrol 容器已重启"
    else
        log "❌ coolcontrol 容器重启失败"
        exit 1
    fi
else
    log "⚠️ 未找到 coolcontrol 容器，跳过重启"
fi
