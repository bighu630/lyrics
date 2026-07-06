# 快速开始

> ⚠️ **该项目已重构为单一 C++17 二进制文件**。以下文档已不再维护，请查阅 [README.md](README.md) 获取最新指南。

## 快速安装

```bash
# 1. 安装依赖
# Arch:    sudo pacman -S cmake gcc pkg-config gtk4 libcurl dbus playerctl mpc
# Ubuntu:  sudo apt install cmake g++ pkg-config libgtk-4-dev libcurl4-openssl-dev libdbus-1-dev playerctl mpc

# 2. 构建
make

# 3. 配置
mkdir -p ~/.config/lyrics
cp config.toml.example ~/.config/lyrics/config.toml
# 编辑 ~/.config/lyrics/config.toml，填入 AI API 密钥

# 4. 运行
./build/lyrics
```

详细说明请查看 [README.md](README.md)。
