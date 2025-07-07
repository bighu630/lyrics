#!/bin/bash
# 歌词应用配置检查脚本
# 用法: ./check_config.sh

set -e

echo "🔍 检查歌词应用配置..."
echo "=================================="

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 检查函数
check_ok() {
    echo -e "${GREEN}✅ $1${NC}"
}

check_warn() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

check_error() {
    echo -e "${RED}❌ $1${NC}"
}

# 检查配置文件
config_found=false
echo ""
echo "📁 配置文件检查:"

for config_file in .env .env.local ~/.lyrics.env; do
    if [[ -f "$config_file" ]]; then
        check_ok "找到配置文件: $config_file"
        config_found=true
        
        # 检查文件权限
        permissions=$(stat -c "%a" "$config_file" 2>/dev/null || stat -f "%A" "$config_file" 2>/dev/null)
        if [[ "$permissions" =~ ^[67][0-7][0-7]$ ]]; then
            check_ok "文件权限安全: $config_file ($permissions)"
        else
            check_warn "建议设置安全权限: chmod 600 $config_file"
        fi
        
        # 检查必需的配置项
        if grep -q "^GEMINI_API_KEY=" "$config_file" && ! grep -q "^GEMINI_API_KEY=$" "$config_file"; then
            check_ok "GEMINI_API_KEY 已设置 (在 $config_file)"
        fi
        
        # 检查可选配置项
        if grep -q "^NETEASE_COOKIE=" "$config_file" && ! grep -q "^NETEASE_COOKIE=$" "$config_file"; then
            check_ok "NETEASE_COOKIE 已设置 (在 $config_file)"
        fi
        
        if grep -q "^LOG_LEVEL=" "$config_file"; then
            log_level=$(grep "^LOG_LEVEL=" "$config_file" | cut -d'=' -f2 | tr -d '"' | tr -d "'")
            if [[ "$log_level" =~ ^(debug|info|warn|error)$ ]]; then
                check_ok "日志级别: $log_level"
            else
                check_warn "日志级别不正确: $log_level (应为: debug, info, warn, error)"
            fi
        fi
        
    else
        check_warn "配置文件不存在: $config_file"
    fi
done

# 检查环境变量
echo ""
echo "🌍 环境变量检查:"

if [[ -n "$GEMINI_API_KEY" ]]; then
    check_ok "GEMINI_API_KEY 环境变量已设置"
elif ! $config_found; then
    check_error "GEMINI_API_KEY 未设置 (必需)"
    echo "   请设置环境变量或创建配置文件"
fi

# 检查缓存目录
echo ""
echo "📦 缓存目录检查:"

cache_dir="${LYRICS_CACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/lyrics}"
if [[ -d "$cache_dir" ]]; then
    if [[ -w "$cache_dir" ]]; then
        check_ok "缓存目录可写: $cache_dir"
        
        # 显示缓存文件数量
        lrc_count=$(find "$cache_dir" -name "*.lrc" 2>/dev/null | wc -l)
        if [[ $lrc_count -gt 0 ]]; then
            check_ok "已缓存歌词文件: $lrc_count 个"
        else
            check_warn "暂无缓存的歌词文件"
        fi
    else
        check_error "缓存目录无写入权限: $cache_dir"
    fi
else
    check_warn "缓存目录不存在，将自动创建: $cache_dir"
fi

# 检查 Socket 路径
echo ""
echo "🔌 IPC Socket 检查:"

socket_path="${SOCKET_PATH:-/tmp/lyrics_app.sock}"
socket_dir=$(dirname "$socket_path")

if [[ -d "$socket_dir" && -w "$socket_dir" ]]; then
    check_ok "Socket 目录可写: $socket_dir"
else
    check_error "Socket 目录不可写: $socket_dir"
fi

if [[ -S "$socket_path" ]]; then
    check_warn "Socket 文件已存在: $socket_path (可能有实例在运行)"
fi

# 检查依赖
echo ""
echo "🛠️  系统依赖检查:"

# 检查 playerctl
if command -v playerctl >/dev/null 2>&1; then
    check_ok "playerctl 已安装: $(playerctl --version)"
else
    check_error "playerctl 未安装 (播放器控制必需)"
fi

# 检查构建工具
if command -v go >/dev/null 2>&1; then
    check_ok "Go 编译器: $(go version)"
else
    check_warn "Go 编译器未安装 (编译后端需要)"
fi

if command -v g++ >/dev/null 2>&1; then
    check_ok "C++ 编译器: $(g++ --version | head -1)"
else
    check_warn "C++ 编译器未安装 (编译前端需要)"
fi

# 检查 GTK4
if pkg-config --exists gtk4 2>/dev/null; then
    gtk_version=$(pkg-config --modversion gtk4)
    check_ok "GTK4 开发库: $gtk_version"
else
    check_warn "GTK4 开发库未安装 (GUI 编译需要)"
fi

# 检查可执行文件
echo ""
echo "🚀 可执行文件检查:"

if [[ -x "./build/lyrics-backend" ]]; then
    check_ok "后端程序已编译: ./build/lyrics-backend"
elif [[ -x "$(which lyrics-backend 2>/dev/null)" ]]; then
    check_ok "后端程序已安装: $(which lyrics-backend)"
else
    check_warn "后端程序未找到，请运行: make backend"
fi

if [[ -x "./build/lyrics-gui" ]]; then
    check_ok "前端程序已编译: ./build/lyrics-gui"
elif [[ -x "$(which lyrics-gui 2>/dev/null)" ]]; then
    check_ok "前端程序已安装: $(which lyrics-gui)"
else
    check_warn "前端程序未找到，请运行: make gui"
fi

# 总结
echo ""
echo "=================================="
echo "🎵 配置检查完成"

if ! $config_found && [[ -z "$GEMINI_API_KEY" ]]; then
    echo ""
    echo "❗ 下一步:"
    echo "   1. 复制配置模板: cp .env.example .env"
    echo "   2. 编辑配置文件: nano .env"
    echo "   3. 设置 GEMINI_API_KEY"
    echo "   4. 重新运行此检查脚本"
fi

echo ""
echo "📖 更多帮助请查看 README.md"
