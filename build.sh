#!/bin/bash

# === 脚本配置 ===
IMAGE_NAME="qnap8528-compiler"
CONTAINER_NAME="qnap8528-container"
SOURCE_CODE_DIR="./src"
MODULE_OUTPUT_DIR="/lib/modules/$(uname -r)/extra"
KERNEL_VERSION=$(uname -r)
KERNEL_HEADERS_DIR="/usr/src/linux-headers-$KERNEL_VERSION"

# === 运行模式 ===
# --auto 模式：跳过所有交互提示，适用于开机自启场景
# --rebuild 模式：强制重新构建 Docker 镜像（用于修复 GCC 版本不匹配等问题）
AUTO_MODE=false
REBUILD=false
for arg in "$@"; do
    case "$arg" in
        --auto) AUTO_MODE=true ;;
        --rebuild) REBUILD=true ;;
    esac
done

# === 前置检查 ===
# 检查 Docker 服务状态
if ! systemctl is-active --quiet docker; then
    echo "❌ 错误：Docker 服务未运行，请先启动 Docker"
    exit 1
fi

# 检查内核头文件是否存在
if [ ! -d "$KERNEL_HEADERS_DIR" ]; then
    echo "❌ 错误：未找到内核头文件 $KERNEL_HEADERS_DIR"
    echo "📦 请先安装对应版本的内核开发包（通常为 linux-headers-$KERNEL_VERSION）"
    exit 1
fi

# === 镜像管理 ===
# 从宿主机 /proc/version 提取内核编译时使用的 GCC 版本
# 格式示例: "x86_64-linux-gnu-gcc (Debian 12.2.0-14) 12.2.0" → 提取 "12.2.0-14"
KERNEL_GCC_VERSION=$(grep -oP '\(Debian \K[0-9]+\.[0-9]+\.[0-9]+-[0-9a-z+.]+' /proc/version)
if [ -z "$KERNEL_GCC_VERSION" ]; then
    echo "⚠️ 警告：无法从 /proc/version 提取 GCC 版本，将使用镜像默认版本"
else
    echo "ℹ️ 检测到内核 GCC 版本: $KERNEL_GCC_VERSION"
fi

build_image() {
    echo "🔄 正在构建 Docker 镜像 ($IMAGE_NAME)..."
    local build_args=""
    if [ -n "$KERNEL_GCC_VERSION" ]; then
        build_args="--build-arg GCC_VERSION=$KERNEL_GCC_VERSION"
        echo "ℹ️ 锁定 GCC 版本为: $KERNEL_GCC_VERSION（与内核编译版本一致）"
    fi
    docker build $build_args -t "$IMAGE_NAME" -f Dockerfile .
    if [ $? -ne 0 ]; then
        echo "❌ 镜像构建失败，请检查 Dockerfile 或网络连接"
        exit 1
    fi
    echo "✅ 镜像构建成功"
}

# 检查并处理镜像
if [ "$REBUILD" = true ]; then
    echo "ℹ️ 强制重建模式：重新构建镜像"
    build_image
elif docker images -q "$IMAGE_NAME" | grep -q .; then
    if [ "$AUTO_MODE" = true ]; then
        echo "ℹ️ 自动模式：直接使用已有镜像"
    else
        read -p "⚠️ 检测到已有镜像，是否重新构建？(y/N): " choice
        [[ $choice =~ ^[Yy]$ ]] && build_image
    fi
else
    build_image
fi

# === 容器管理 ===
clean_container() {
    if docker ps -a --format '{{.Names}}' | grep -q "^$CONTAINER_NAME$"; then
        echo "🧹 清理旧容器 $CONTAINER_NAME..."
        docker stop "$CONTAINER_NAME" >/dev/null 2>&1
        docker rm "$CONTAINER_NAME" >/dev/null 2>&1
    fi
}

start_container() {
    clean_container
    echo "🚀 启动 Docker 容器..."
    local docker_flags="-td"
    [ "$AUTO_MODE" = true ] && docker_flags="-td"  # 非交互模式不加 -i
    docker run $docker_flags \
        --name "$CONTAINER_NAME" \
        -v "$SOURCE_CODE_DIR:/driver" \
        -v "$KERNEL_HEADERS_DIR:/usr/src/linux-headers" \
        "$IMAGE_NAME" bash
    if [ $? -ne 0 ]; then
        echo "❌ 容器启动失败，请检查挂载路径或镜像完整性"
        exit 1
    fi
}

# === GCC 版本验证 ===
verify_gcc() {
    if [ -z "$KERNEL_GCC_VERSION" ]; then
        return 0
    fi
    local container_gcc_full
    container_gcc_full=$(docker exec "$CONTAINER_NAME" gcc --version 2>/dev/null | head -1)
    echo "ℹ️ 容器内 GCC: $container_gcc_full"
    # 从 gcc --version 输出中提取 Debian 包版本号（如 "12.2.0-14+deb12u1"）
    local container_gcc
    container_gcc=$(echo "$container_gcc_full" | sed -n 's/.*Debian \([0-9][0-9.]*-[0-9][0-9a-z+.]*\).*/\1/p')
    if [ -z "$container_gcc" ]; then
        echo "❌ 无法从 GCC 输出中提取版本: $container_gcc_full"
        return 1
    fi
    echo "ℹ️ 版本对比: 内核=$KERNEL_GCC_VERSION, 容器=$container_gcc"
    # 提取上游版本号（"." 和 "-" 分隔的前三段，如 "12.2.0-14" → "12.2.0"）
    local kernel_upstream container_upstream
    kernel_upstream=$(echo "$KERNEL_GCC_VERSION" | awk -F'[.-]' '{print $1"."$2"."$3}')
    container_upstream=$(echo "$container_gcc" | awk -F'[.-]' '{print $1"."$2"."$3}')
    if [ "$kernel_upstream" != "$container_upstream" ]; then
        echo "❌ GCC 上游版本不匹配：内核=$kernel_upstream, 容器=$container_upstream"
        return 1
    fi
    if [ "$KERNEL_GCC_VERSION" != "$container_gcc" ]; then
        echo "⚠️ GCC Debian 版本有差异：内核=$KERNEL_GCC_VERSION, 容器=$container_gcc"
        echo "   上游版本一致，模块加载应该不受影响"
    fi
    echo "✅ GCC 版本验证通过"
}

# === 编译流程 ===
compile_driver() {
    echo "🔨 开始编译内核模块..."
    docker exec -t "$CONTAINER_NAME" bash -c "
        cd /driver && \
        make -C /usr/src/linux-headers M=\$PWD clean && \
        make -C /usr/src/linux-headers M=\$PWD modules
    "
    if [ $? -ne 0 ]; then
        echo "❌ 编译失败，请检查代码或内核头文件兼容性"
        clean_container
        exit 1
    fi
    echo "✅ 编译成功"
}

# === 安装流程 ===
install_driver() {
    echo "📦 安装驱动到系统..."
    sudo mkdir -p "$MODULE_OUTPUT_DIR"
    sudo cp "$SOURCE_CODE_DIR"/*.ko "$MODULE_OUTPUT_DIR"
    sudo depmod -a

    # 诊断模块信息
    local ko_file="$SOURCE_CODE_DIR/qnap8528.ko"
    if [ -f "$ko_file" ]; then
        echo "📋 模块诊断信息："
        echo "   文件: $(file "$ko_file")"
        echo "   vermagic: $(modinfo -F vermagic "$ko_file" 2>/dev/null)"
        echo "   当前内核: $(uname -r)"
    fi

    local driver_name=$(basename "$SOURCE_CODE_DIR"/*.ko .ko)
    if ! sudo modprobe "$driver_name" 2>&1; then
        echo "❌ 驱动加载失败，dmesg 信息："
        sudo dmesg | tail -5
        return 1
    fi
    echo "✅ 驱动 $driver_name 已加载"

    # 配置 systemd 开机自启（替代旧的 /etc/modules-load.d 方式）
    cat <<EOF | sudo tee /etc/systemd/system/qnap8528-load.service >/dev/null
[Unit]
Description=Load qnap8528 Kernel Module
After=syslog.target network.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/sbin/modprobe $driver_name
ExecStop=/sbin/modprobe -r $driver_name

[Install]
WantedBy=multi-user.target
EOF
    sudo systemctl enable --now qnap8528-load.service >/dev/null
    echo "⚙️ 已配置开机自动加载"
}

# === 清理资源 ===
clean_resources() {
    echo "🧹 清理临时容器..."
    docker stop "$CONTAINER_NAME" >/dev/null 2>&1
    docker rm "$CONTAINER_NAME" >/dev/null 2>&1
}

# === 传感器检测 ===
check_sensors() {
    echo "📊 检测传感器数据..."
    if ! command -v sensors &> /dev/null; then
        echo "⚠️ 警告：sensors 工具未安装，跳过传感器检测"
        return
    fi
    local sensor_data=$(sensors | awk '/qnap8528/,/^$/')
    if [ -n "$sensor_data" ]; then
        echo "🌡️ qnap8528 传感器信息："
        echo "$sensor_data"
    else
        echo "❌ 未检测到 qnap8528 传感器数据（可能驱动未正确加载）"
    fi
}

# === 主流程 ===
main() {
    start_container
    if ! verify_gcc; then
        clean_container
        exit 1
    fi
    compile_driver
    local install_ok=true
    install_driver || install_ok=false
    clean_resources
    if [ "$install_ok" = false ]; then
        exit 1
    fi
    check_sensors
}

# 以 root 权限执行核心操作
if [ "$EUID" -ne 0 ]; then
    echo "🔒 请使用 root 权限运行脚本（sudo ./build.sh）"
    exit 1
fi

main