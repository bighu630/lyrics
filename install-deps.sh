#!/bin/bash
# filepath: /data/code/lyrics/install-deps.sh

# 智能歌词显示系统 - 依赖安装脚本
# 支持 Arch Linux, Ubuntu/Debian, Fedora/CentOS

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印函数
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检测发行版
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        VERSION=$VERSION_ID
    elif [ -f /etc/arch-release ]; then
        DISTRO="arch"
    elif [ -f /etc/debian_version ]; then
        DISTRO="debian"
    elif [ -f /etc/redhat-release ]; then
        DISTRO="rhel"
    else
        DISTRO="unknown"
    fi
    
    print_info "检测到发行版: $DISTRO"
}

# 检查命令是否存在
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# 检查包是否已安装
check_package() {
    local package=$1
    case $DISTRO in
        "arch"|"manjaro"|"endeavouros")
            pacman -Q "$package" >/dev/null 2>&1
            ;;
        "ubuntu"|"debian"|"pop"|"linuxmint")
            dpkg -l "$package" >/dev/null 2>&1
            ;;
        "fedora"|"centos"|"rhel"|"rocky"|"almalinux")
            rpm -q "$package" >/dev/null 2>&1
            ;;
        *)
            return 1
            ;;
    esac
}

# 安装 Arch Linux 依赖
install_arch() {
    print_info "安装 Arch Linux 依赖..."
    
    # 更新包数据库
    sudo pacman -Sy
    
    local packages=(
        "go"                # Go 编程语言
        "gtk4"              # GTK4 库
        "pkg-config"        # pkg-config 工具
        "playerctl"         # 媒体播放器控制
        "gcc"               # C++ 编译器
        "make"              # Make 构建工具
        "socat"             # IPC 通信工具
        "curl"              # HTTP 客户端
        "git"               # 版本控制
    )
    
    local missing_packages=()
    for package in "${packages[@]}"; do
        if ! check_package "$package"; then
            missing_packages+=("$package")
        else
            print_success "$package 已安装"
        fi
    done
    
    if [ ${#missing_packages[@]} -gt 0 ]; then
        print_info "安装缺失的包: ${missing_packages[*]}"
        sudo pacman -S --needed "${missing_packages[@]}"
    fi
}

# 安装 Ubuntu/Debian 依赖
install_debian() {
    print_info "安装 Ubuntu/Debian 依赖..."
    
    # 更新包列表
    sudo apt update
    
    local packages=(
        "golang-go"         # Go 编程语言
        "libgtk-4-dev"      # GTK4 开发库
        "pkg-config"        # pkg-config 工具
        "playerctl"         # 媒体播放器控制
        "build-essential"   # 构建工具
        "socat"             # IPC 通信工具
        "curl"              # HTTP 客户端
        "git"               # 版本控制
    )
    
    local missing_packages=()
    for package in "${packages[@]}"; do
        if ! check_package "$package"; then
            missing_packages+=("$package")
        else
            print_success "$package 已安装"
        fi
    done
    
    if [ ${#missing_packages[@]} -gt 0 ]; then
        print_info "安装缺失的包: ${missing_packages[*]}"
        sudo apt install -y "${missing_packages[@]}"
    fi
}

# 安装 Fedora/RHEL 依赖
install_fedora() {
    print_info "安装 Fedora/RHEL 依赖..."
    
    local packages=(
        "golang"            # Go 编程语言
        "gtk4-devel"        # GTK4 开发库
        "pkgconf-pkg-config" # pkg-config 工具
        "playerctl"         # 媒体播放器控制
        "gcc-c++"           # C++ 编译器
        "make"              # Make 构建工具
        "socat"             # IPC 通信工具
        "curl"              # HTTP 客户端
        "git"               # 版本控制
    )
    
    local missing_packages=()
    for package in "${packages[@]}"; do
        if ! check_package "$package"; then
            missing_packages+=("$package")
        else
            print_success "$package 已安装"
        fi
    done
    
    if [ ${#missing_packages[@]} -gt 0 ]; then
        print_info "安装缺失的包: ${missing_packages[*]}"
        if command_exists dnf; then
            sudo dnf install -y "${missing_packages[@]}"
        else
            sudo yum install -y "${missing_packages[@]}"
        fi
    fi
}

# 验证安装
verify_installation() {
    print_info "验证安装..."
    
    local required_commands=("go" "pkg-config" "playerctl" "gcc" "make" "socat" "curl" "git")
    local missing_commands=()
    
    for cmd in "${required_commands[@]}"; do
        if command_exists "$cmd"; then
            print_success "$cmd 可用"
        else
            missing_commands+=("$cmd")
            print_error "$cmd 未找到"
        fi
    done
    
    # 检查 GTK4
    if pkg-config --exists gtk4; then
        local gtk_version=$(pkg-config --modversion gtk4)
        print_success "GTK4 可用 (版本: $gtk_version)"
    else
        missing_commands+=("gtk4")
        print_error "GTK4 开发库未找到"
    fi
    
    # 检查 Go 版本
    if command_exists go; then
        local go_version=$(go version | cut -d' ' -f3)
        print_success "Go 版本: $go_version"
        
        # 检查 Go 版本是否满足要求
        local go_major=$(echo $go_version | sed 's/go//g' | cut -d'.' -f1)
        local go_minor=$(echo $go_version | sed 's/go//g' | cut -d'.' -f2)
        
        if [ "$go_major" -lt 1 ] || ([ "$go_major" -eq 1 ] && [ "$go_minor" -lt 19 ]); then
            print_warning "Go 版本可能过低，推荐 1.19 或更高版本"
        fi
    fi
    
    if [ ${#missing_commands[@]} -eq 0 ]; then
        print_success "所有依赖安装成功！"
        return 0
    else
        print_error "缺少以下依赖: ${missing_commands[*]}"
        return 1
    fi
}

# 设置 Go 环境
setup_go_env() {
    print_info "设置 Go 环境..."
    
    # 检查 GOPATH 和 GOROOT
    if [ -z "$GOPATH" ]; then
        export GOPATH="$HOME/go"
        echo 'export GOPATH="$HOME/go"' >> ~/.bashrc
        print_info "设置 GOPATH=$GOPATH"
    fi
    
    # 确保 Go bin 在 PATH 中
    if ! echo "$PATH" | grep -q "$GOPATH/bin"; then
        export PATH="$PATH:$GOPATH/bin"
        echo 'export PATH="$PATH:$GOPATH/bin"' >> ~/.bashrc
        print_info "添加 Go bin 目录到 PATH"
    fi
    
    # 创建 Go 工作目录
    mkdir -p "$GOPATH/src" "$GOPATH/bin" "$GOPATH/pkg"
}

# 安装额外工具
install_extra_tools() {
    print_info "安装额外开发工具..."
    
    # 安装 Go 模块依赖（如果 go.mod 存在）
    if [ -f "go.mod" ]; then
        print_info "检测到 go.mod，下载 Go 模块依赖..."
        go mod download
        go mod tidy
    fi
}

# 显示后续步骤
show_next_steps() {
    print_success "依赖安装完成！"
    echo
    print_info "后续步骤:"
    echo "1. 创建配置文件:"
    echo "   mkdir -p ~/.config/lyrics"
    echo "   cp config.toml.example ~/.config/lyrics/config.toml"
    echo "   nano ~/.config/lyrics/config.toml  # 设置 Gemini API 密钥"
    echo
    echo "2. 编译项目:"
    echo "   make all"
    echo
    echo "3. 安装到用户目录:"
    echo "   make install-user"
    echo
    echo "4. 启动应用:"
    echo "   lyrics-backend &"
    echo "   lyrics-gui"
    echo
    print_info "详细说明请查看: README.md 或 QUICK_START.md"
}

# 主函数
main() {
    echo "========================================"
    echo "   智能歌词显示系统 - 依赖安装脚本"
    echo "========================================"
    echo
    
    # 检查是否为 root 用户
    if [ "$EUID" -eq 0 ]; then
        print_error "请不要以 root 用户运行此脚本"
        exit 1
    fi
    
    # 检测发行版
    detect_distro
    
    # 根据发行版安装依赖
    case $DISTRO in
        "arch"|"manjaro"|"endeavouros")
            install_arch
            ;;
        "ubuntu"|"debian"|"pop"|"linuxmint")
            install_debian
            ;;
        "fedora"|"centos"|"rhel"|"rocky"|"almalinux")
            install_fedora
            ;;
        *)
            print_error "不支持的发行版: $DISTRO"
            print_info "请手动安装以下依赖:"
            echo "- Go (>=1.19)"
            echo "- GTK4 开发库"
            echo "- pkg-config"
            echo "- playerctl"
            echo "- build-essential/gcc"
            echo "- socat"
            echo "- curl"
            echo "- git"
            exit 1
            ;;
    esac
    
    # 设置 Go 环境
    setup_go_env
    
    # 安装额外工具
    install_extra_tools
    
    # 验证安装
    if verify_installation; then
        show_next_steps
        exit 0
    else
        print_error "依赖安装失败，请检查错误信息"
        exit 1
    fi
}

# 运行主函数
main "$@"