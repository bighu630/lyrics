# 智能歌词显示系统

一个基于 Hyprland 的实时歌词显示系统，能够自动获取当前播放的媒体信息，通过 AI 分析判断是否为歌曲，然后获取歌词并在透明窗口中实时显示。
![image](https://github.com/user-attachments/assets/84d96bf3-fcd6-48f1-8cd7-864d0003861a)


## 📋 目录

- [系统架构](#系统架构)
- [工作流程](#工作流程)
- [技术栈](#技术栈)
- [快速开始](#快速开始)
- [详细配置](#详细配置)
- [项目结构](#项目结构)
- [Hyprland 集成](#hyprland-集成)
- [常见问题](#常见问题)
- [开发说明](#开发说明)

## 🏗️ 系统架构

```mermaid
graph TD
    Player[播放器应用\nSpotify/Chrome/VLC]
    PlayerCtl[playerctl\nMPRIS 接口]

    Backend[Go 后端服务]
    MediaGet[媒体信息获取]
    AIAnalysis[AI 歌曲分析\nGemini API]
    LyricsGet[歌词获取\n网易云 API]
    LRCParser[LRC 解析器\n时间同步]
    IPCServer[IPC 服务器\n实时推送]

    Cache[LRC 缓存文件\n本地存储]

    GUI[GTK4 前端窗口]
    Overlay[透明歌词窗口\n实时显示]
    IPCClient[IPC 客户端\n接收歌词]

    GeminiAPI[Gemini AI API]
    NeteaseAPI[网易云音乐 API]

    Player ---|MPRIS| PlayerCtl
    PlayerCtl --> MediaGet
    MediaGet --> AIAnalysis
    AIAnalysis <--> GeminiAPI
    AIAnalysis --> LyricsGet
    LyricsGet <--> NeteaseAPI
    LyricsGet --> Cache
    Cache --> LRCParser
    LRCParser --> IPCServer

    IPCServer ---|Unix Socket| IPCClient
    IPCClient --> Overlay
```

### 🔄 数据流向说明

1. **媒体监听** → 播放器通过 MPRIS 标准暴露媒体信息
2. **信息获取** → playerctl 获取当前播放的媒体标题和艺术家
3. **AI 分析** → Gemini AI 判断是否为歌曲并提取标准化信息
4. **歌词获取** → 通过网易云 API 搜索并下载歌词
5. **本地缓存** → 歌词保存为 LRC 文件，避免重复下载
6. **解析同步** → 解析 LRC 时间戳，实现毫秒级同步
7. **实时推送** → 通过 IPC 将当前歌词行推送给前端
8. **界面显示** → GTK4 透明窗口实时显示歌词

## 🔄 工作流程

### 1. 媒体信息获取
- 使用 `playerctl` 监听当前播放的媒体信息
- 获取媒体标题、艺术家等元数据
- 支持所有符合 MPRIS 标准的播放器 (Spotify, Chrome, Firefox, VLC 等)

### 2. AI 歌曲分析
- 将媒体标题发送给 Gemini AI 进行分析
- AI 判断是否为歌曲类型的媒体
- 提取标准化的歌曲名和歌手信息
- 过滤掉非音乐内容 (视频、播客等)

### 3. 歌词获取与缓存
- 使用通用音乐API接口支持多个音乐平台
- 主要使用网易云音乐API，支持自动故障转移
- 未来可扩展支持QQ音乐、酷狗音乐等平台
- 支持多种歌词格式 (LRC 带时间戳格式)
- 自动缓存歌词文件到本地 `lyrics_cache/` 目录
- 避免重复下载，提高响应速度

### 4. LRC 歌词解析
- 解析 LRC 格式的时间戳和歌词内容
- 支持毫秒级精度的时间同步
- 处理歌词的特殊格式和编码

### 5. 实时推送与显示
- 通过 Unix Domain Socket (IPC) 实时推送当前歌词行
- GTK4 透明窗口接收歌词并实时更新显示
- 支持自动重连和错误恢复

## 🛠️ 技术栈

### 后端 (Go)
- **playerctl**: 媒体播放器控制和信息获取
- **Gemini AI API**: 智能歌曲识别和信息提取
- **音乐API管理器**: 支持多个音乐平台的统一接口
- **网易云音乐 API**: 主要歌词数据源
- **QQ音乐 API**: 备选歌词数据源（开发中）
- **Unix Domain Socket**: 高性能 IPC 通信
- **zerolog**: 结构化日志记录

### 前端 (C++/GTK4)
- **GTK4**: 现代 UI 框架
- **透明窗口**: 无干扰的歌词覆盖显示
- **CSS 样式**: 美观的歌词外观
- **自动重连**: 稳定的后端连接

### 系统集成
- **Hyprland**: Wayland 合成器支持
- **MPRIS**: 媒体播放器标准接口
- **systemd**: 可选的服务管理

## 🚀 快速开始

### 1. 安装依赖
```bash
# 一键安装所有依赖
./install-deps.sh

# 或者手动安装
sudo pacman -S go gtk4 pkg-config playerctl  # Arch Linux
sudo apt install golang-go libgtk-4-dev pkg-config playerctl  # Ubuntu/Debian
```

### 2. 配置 API 密钥

#### 方法一：使用配置文件 (推荐)
```bash
# 复制示例配置文件
cp .env.example .env

# 编辑配置文件
nano .env  # 或使用你喜欢的编辑器
```

#### 方法二：直接设置环境变量
```bash
# 临时设置 (仅当前会话)
export GEMINI_API_KEY="your_gemini_api_key_here"

# 永久设置 (添加到 ~/.bashrc 或 ~/.zshrc)
echo 'export GEMINI_API_KEY="your_gemini_api_key_here"' >> ~/.bashrc
source ~/.bashrc
```

#### 方法三：用户全局配置文件
```bash
# 创建用户全局配置 (所有项目共享)
nano ~/.lyrics.env
```

**`.env` 配置文件模板**：
```bash
# Lyrics 应用环境变量配置文件
# 文件位置: .env (项目根目录) 或 ~/.lyrics.env (用户主目录)

# =============================================================================
# 必需配置
# =============================================================================

# Gemini AI API 密钥 
# 获取地址: https://makersuite.google.com/app/apikey
GEMINI_API_KEY=your_gemini_api_key_here

# =============================================================================
# 可选配置
# =============================================================================

# 网易云音乐 Cookie (可选，用于避免限流)
# 获取方式: 浏览器开发者工具 -> 网络 -> music.163.com -> 请求头中的 Cookie
NETEASE_COOKIE=""

# 自定义缓存目录 (可选)
# 默认: ~/.cache/lyrics (Linux) 或 $XDG_CACHE_HOME/lyrics
LYRICS_CACHE_DIR=""

# IPC Socket 路径 (可选)
# 默认: /tmp/lyrics_app.sock
SOCKET_PATH=""

# 日志级别 (可选)
# 可选值: debug, info, warn, error
# 默认: info
LOG_LEVEL=info

# Gemini 模型名称 (可选)
# 推荐: gemini-2.5-flash (更快) 或 gemini-pro (更准确)
# 默认: gemini-pro
GEMINI_MODEL=gemini-2.5-flash
```

#### 如何获取 Gemini API 密钥

1. 访问 [Google AI Studio](https://makersuite.google.com/app/apikey)
2. 登录 Google 账号
3. 点击 "Create API Key"
4. 复制生成的 API 密钥
5. 将密钥添加到配置文件中

#### 如何获取网易云 Cookie (可选)

网易云 Cookie 可以提高歌词获取成功率，避免被限流：

1. 浏览器打开 [music.163.com](https://music.163.com)
2. 登录你的网易云账号
3. 按 F12 打开开发者工具
4. 切换到 "Network/网络" 标签
5. 刷新页面，找到 music.163.com 的请求
6. 在请求头中找到 `Cookie` 字段
7. 复制整个 Cookie 值到配置文件中

### 3. 编译项目
```bash
# 编译后端和前端
make all

# 或者单独编译
make backend  # 只编译 Go 后端
make gui      # 只编译 GTK4 前端
```

### 4. 运行系统
```bash
# 启动后端服务 (在后台运行)
make run-backend &

# 启动 GUI 客户端 (透明窗口模式)
make run-gui

# 或者控制台模式 (用于调试)
make run-gui-console
```

### 5. 安装到系统 (可选)
```bash
# 系统安装 (需要 sudo 权限，安装到 /usr/local/bin)
sudo make install

# 用户安装 (推荐，安装到 ~/.local/bin)
make install-user

# 确保 ~/.local/bin 在 PATH 中 (仅用户安装需要)
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc  # 或重新打开终端

# 安装后可以在任何地方运行
lyrics-backend &     # 启动后端
lyrics-gui          # 启动GUI
lyrics-gui -c       # 控制台模式
```

### 6. 卸载 (如果需要)
```bash
# 卸载系统安装
sudo make uninstall

# 卸载用户安装
make uninstall-user
```

## ⚙️ 详细配置

### 环境变量配置

应用支持多种方式配置环境变量，按优先级排序：

1. **命令行环境变量** (最高优先级)
2. **配置文件** (推荐方式)
3. **默认值** (最低优先级)

### 配置文件支持

应用会按以下顺序自动加载配置文件：

```bash
.env          # 项目根目录配置文件
.env.local    # 本地开发配置文件  
~/.lyrics.env # 用户主目录配置文件
```

**示例配置文件 (`.env`)**：
```bash
# === 必需配置 ===
# Gemini AI API 密钥
GEMINI_API_KEY=your_gemini_api_key_here

# === 可选配置 ===
# 网易云音乐 Cookie (避免限流)
NETEASE_COOKIE="your_netease_cookie_here"

# 自定义缓存目录
LYRICS_CACHE_DIR=/path/to/custom/cache

# IPC Socket 路径
SOCKET_PATH=/tmp/custom_lyrics_app.sock

# 日志级别 (debug, info, warn, error)
LOG_LEVEL=info

# Gemini 模型版本
GEMINI_MODEL=gemini-2.5-flash
```

### 环境变量说明

| 变量名 | 描述 | 默认值 | 是否必需 |
|--------|------|--------|----------|
| `GEMINI_API_KEY` | Gemini AI API 密钥 | 无 | ✅ 必需 |
| `NETEASE_COOKIE` | 网易云音乐 Cookie | 空 | ❌ 可选 |
| `LYRICS_CACHE_DIR` | 歌词缓存目录 | `~/.cache/lyrics` | ❌ 可选 |
| `SOCKET_PATH` | IPC Socket 路径 | `/tmp/lyrics_app.sock` | ❌ 可选 |
| `LOG_LEVEL` | 日志级别 | `info` | ❌ 可选 |
| `GEMINI_MODEL` | Gemini 模型名称 | `gemini-pro` | ❌ 可选 |

### 配置文件格式

```bash
# 注释行以 # 开头
KEY=value
KEY="quoted value"
KEY='single quoted value'

# 空行会被忽略

# 支持的引号格式
QUOTED_VALUE="双引号值"
SINGLE_QUOTED='单引号值'
UNQUOTED_VALUE=无引号值
```

### 配置验证与故障排除

#### 验证配置
启动应用时会显示配置加载信息：
```bash
./build/lyrics-backend
# 输出示例:
# 2024/12/19 10:30:00 [INFO] Loaded environment file: .env
# 2024/12/19 10:30:00 [INFO] Cache directory: /home/user/.cache/lyrics
# 2024/12/19 10:30:00 [INFO] Socket path: /tmp/lyrics_app.sock
# 2024/12/19 10:30:00 [INFO] Starting lyrics app...
```

#### 检查配置是否正确
```bash
# 方法一：查看应用启动日志
./build/lyrics-backend | head -10

# 方法二：测试环境变量
echo $GEMINI_API_KEY  # 应该显示你的API密钥

# 方法三：检查配置文件是否存在
ls -la .env ~/.lyrics.env 2>/dev/null || echo "未找到配置文件"
```

#### 常见配置问题

**问题 1: GEMINI_API_KEY 未设置**
```
错误: API key is required
解决: 确保在 .env 文件中设置了正确的 GEMINI_API_KEY
```

**问题 2: 配置文件格式错误**
```
错误: 配置加载失败
解决: 检查 .env 文件格式，确保没有多余的空格和引号
正确格式: GEMINI_API_KEY=your_key_here
错误格式: GEMINI_API_KEY = "your_key_here"
```

**问题 3: 缓存目录权限问题**
```bash
# 检查缓存目录权限
ls -ld ~/.cache/lyrics

# 如果目录不存在或权限不足，重新创建
rm -rf ~/.cache/lyrics
mkdir -p ~/.cache/lyrics
chmod 755 ~/.cache/lyrics
```

**问题 4: Socket 文件权限问题**
```bash
# 清理旧的 socket 文件
rm -f /tmp/lyrics_app.sock

# 检查 /tmp 目录权限
ls -ld /tmp
```

#### 配置优先级说明

配置加载顺序 (后者覆盖前者)：
```
1. 默认值
2. ~/.lyrics.env (用户全局配置)
3. .env.local (本地开发配置)  
4. .env (项目配置)
5. 命令行环境变量 (最高优先级)
```

**示例**：
```bash
# ~/.lyrics.env
GEMINI_API_KEY=global_key
LOG_LEVEL=info

# .env
GEMINI_API_KEY=project_key
LYRICS_CACHE_DIR=/custom/cache

# 命令行
LYRICS_CACHE_DIR=/tmp/cache ./build/lyrics-backend

# 最终配置:
# GEMINI_API_KEY=project_key (来自 .env)
# LOG_LEVEL=info (来自 ~/.lyrics.env)  
# LYRICS_CACHE_DIR=/tmp/cache (来自命令行)
```

### 配置管理最佳实践

#### 配置安全
```bash
# 1. 设置配置文件权限 (仅自己可读)
chmod 600 ~/.lyrics.env
chmod 600 .env

# 2. 避免在版本控制中暴露密钥
echo ".env" >> .gitignore
echo ".env.local" >> .gitignore

# 3. 使用环境变量而非硬编码
# ❌ 不推荐: 直接写在代码里
# ✅ 推荐: 使用配置文件或环境变量
```

#### 配置备份与迁移
```bash
# 备份配置到安全位置
cp ~/.lyrics.env ~/Documents/lyrics_config_backup.env

# 迁移到新环境
scp ~/.lyrics.env user@newhost:~/.lyrics.env

# 团队共享配置模板 (移除敏感信息)
cp .env .env.template
sed -i 's/=.*/=your_key_here/' .env.template
```

#### 配置版本管理
```bash
# 项目配置示例文件 (可以提交到 git)
# .env.example
GEMINI_API_KEY=your_gemini_api_key_here
NETEASE_COOKIE=your_netease_cookie_here
LOG_LEVEL=info

# 个人配置 (不提交到 git)
# .env
GEMINI_API_KEY=actual_secret_key
NETEASE_COOKIE=actual_cookie_value
LOG_LEVEL=debug
```

#### 多环境配置切换
```bash
# 开发环境
cp .env.development .env

# 测试环境  
cp .env.testing .env

# 生产环境
cp .env.production .env

# 或使用符号链接
ln -sf .env.production .env
```

#### 配置验证脚本
```bash
#!/bin/bash
# check_config.sh - 验证配置是否正确

echo "🔍 检查配置文件..."

# 检查配置文件是否存在
for config_file in .env ~/.lyrics.env; do
    if [[ -f "$config_file" ]]; then
        echo "✅ 找到配置文件: $config_file"
        
        # 检查必需的配置项
        if grep -q "GEMINI_API_KEY=" "$config_file"; then
            echo "✅ GEMINI_API_KEY 已设置"
        else
            echo "❌ GEMINI_API_KEY 未设置"
        fi
    else
        echo "⚠️  配置文件不存在: $config_file"
    fi
done

# 检查缓存目录
cache_dir="${LYRICS_CACHE_DIR:-$HOME/.cache/lyrics}"
if [[ -d "$cache_dir" && -w "$cache_dir" ]]; then
    echo "✅ 缓存目录可写: $cache_dir"
else
    echo "⚠️  缓存目录问题: $cache_dir"
fi

echo "🎵 配置检查完成"
```

## 📁 项目结构

```
lyrics/
├── go-backend/                 # Go 后端服务
│   ├── cmd/lyrics-backend/     # 主程序入口
│   │   ├── main.go            # 后端主程序
│   │   └── lyrics_cache/      # 歌词缓存目录
│   ├── internal/               # 内部包
│   │   ├── app/               # 应用程序核心逻辑
│   │   ├── config/            # 配置管理
│   │   ├── ipc/               # IPC 服务器
│   │   ├── lyrics/            # 歌词处理和 LRC 解析
│   │   └── player/            # 播放器接口 (playerctl)
│   └── pkg/                   # 外部包
│       ├── ai/                # AI 客户端 (Gemini)
│       ├── music/             # 音乐API通用接口和管理器
│       ├── netease/           # 网易云音乐 API 客户端
│       └── qqmusic/           # QQ音乐 API 客户端（模板）
│
├── cpp-gui/                   # C++ GTK4 前端
│   └── main.cpp              # GUI 主程序和 IPC 客户端
│
├── build/                     # 编译输出目录
│   ├── lyrics-backend         # Go 后端可执行文件
│   └── lyrics-gui             # C++ 前端可执行文件
│
├── lyrics_cache/             # 全局歌词缓存目录 (或 ~/.cache/lyrics/)
├── Makefile                  # 构建和运行脚本
├── simple_ipc_reader.sh     # IPC 测试工具
├── ipc_reader.sh            # IPC 调试工具
├── install-deps.sh          # 依赖安装脚本
├── check_config.sh          # 配置检查脚本
├── README.md                # 主要文档 (本文件)
├── README-GUI.md            # GUI 详细说明
└── MUSIC_API_REFACTORING.md # 音乐API重构说明

# 安装后的系统文件位置:
# /usr/local/bin/lyrics-backend    # 系统安装 (sudo make install)
# /usr/local/bin/lyrics-gui        # 系统安装 (sudo make install)
# ~/.local/bin/lyrics-backend      # 用户安装 (make install-user)
# ~/.local/bin/lyrics-gui          # 用户安装 (make install-user)
# ~/.cache/lyrics/                 # 用户歌词缓存目录
```

## 🖥️ Hyprland 集成

### 1. 窗口规则配置
在 `~/.config/hypr/hyprland.conf` 中添加：

```bash
# 歌词窗口规则 - 使用应用程序类名
windowrulev2 = float, class:^(lyrics-gui)$
windowrulev2 = pin, class:^(lyrics-gui)$
windowrulev2 = stayfocused, class:^(lyrics-gui)$
windowrulev2 = noborder, class:^(lyrics-gui)$
windowrulev2 = noshadow, class:^(lyrics-gui)$
windowrulev2 = noblur, class:^(lyrics-gui)$

# 可选：设置歌词窗口位置和大小
windowrulev2 = move 10% 80%, class:^(lyrics-gui)$
windowrulev2 = size 80% 120, class:^(lyrics-gui)$

# 备用规则 - 使用窗口标题
windowrulev2 = float, title:^(Lyrics Overlay)$
windowrulev2 = pin, title:^(Lyrics Overlay)$
```

### 2. 自动启动
```bash
# 方法一：使用本地安装路径 (开发环境)
exec-once = cd /path/to/lyrics && make run-backend
exec-once = sleep 2 && cd /path/to/lyrics && make run-gui

# 方法二：使用已安装的二进制文件 (推荐)
exec-once = lyrics-backend
exec-once = sleep 2 && lyrics-gui
```

### 3. 快捷键绑定
```bash
# 方法一：使用本地路径 (开发环境)
# 切换歌词显示
bind = SUPER, L, exec, pkill lyrics-gui || (cd /path/to/lyrics && make run-gui)

# 重启歌词服务
bind = SUPER SHIFT, L, exec, pkill lyrics-backend && sleep 1 && (cd /path/to/lyrics && make run-backend)

# 重启整个歌词系统
bind = SUPER CTRL, L, exec, pkill lyrics-backend; pkill lyrics-gui; sleep 1; cd /path/to/lyrics && make run-backend && sleep 2 && make run-gui

# 方法二：使用已安装的二进制文件 (推荐)
# 切换歌词显示
bind = SUPER, L, exec, pkill lyrics-gui || lyrics-gui

# 重启歌词服务
bind = SUPER SHIFT, L, exec, pkill lyrics-backend && sleep 1 && lyrics-backend

# 重启整个歌词系统
bind = SUPER CTRL, L, exec, pkill lyrics-backend; pkill lyrics-gui; sleep 1; lyrics-backend & sleep 2 && lyrics-gui
```

## 🐛 常见问题

### Q: 歌词窗口不显示或显示异常
A: 检查以下几点：
1. 确保 GTK4 正确安装：`pkg-config --modversion gtk4`
2. 检查 Wayland 环境变量：`echo $WAYLAND_DISPLAY`
3. 查看日志输出：运行 `make run-gui-console` 查看详细日志

### Q: 无法连接到后端服务
A: 检查后端服务状态：
```bash
# 检查后端是否运行
ps aux | grep lyrics-backend

# 检查 IPC socket 文件
ls -la /tmp/lyrics_app.sock

# 手动启动后端并查看日志
make run-backend
```

### Q: Gemini AI 无法响应
A: 验证 API 配置：
```bash
# 检查 API 密钥
echo $GEMINI_API_KEY

# 测试网络连接
curl -H "x-goog-api-key: $GEMINI_API_KEY" \
  "https://generativelanguage.googleapis.com/v1beta/models"
```

### Q: 获取不到歌词
A: 检查播放器和网络：
```bash
# 测试 playerctl
playerctl metadata title

# 检查网络连接
curl -I "https://music.163.com"

# 查看缓存目录
ls -la lyrics_cache/
```

## 🔧 开发说明

### 常用命令
```bash
# 编译所有组件
make all

# 运行后端 (后台)
make run-backend

# 运行 GUI (透明窗口)
make run-gui

# 运行 GUI (控制台模式，用于调试)
make run-console

# 安装到系统目录 (需要 sudo)
sudo make install

# 安装到用户目录 (推荐)
make install-user

# 卸载系统安装
sudo make uninstall

# 卸载用户安装
make uninstall-user

# 清理编译文件
make clean

# 查看帮助信息
make help
```

### 调试工具
```bash
# 使用简单的 IPC 读取器测试后端
./simple_ipc_reader.sh

# 查看详细日志
LOG_LEVEL=debug make run-backend
```

### 测试环境
- 确保有音乐播放器在运行 (Spotify, Chrome 播放音乐等)
- 确保网络连接正常
- 确保 Gemini API 密钥有效

### 验证安装
```bash
# 检查依赖
pkg-config --modversion gtk4
playerctl --version
go version

# 检查窗口类名 (运行 GUI 后)
hyprctl clients | grep -A 5 "lyrics"

# 测试 IPC 连接
./simple_ipc_reader.sh
```

### 系统要求
- **操作系统**: Linux (推荐 Arch Linux)
- **桌面环境**: Hyprland (Wayland)
- **Go 版本**: >= 1.19
- **GTK 版本**: GTK4
- **网络**: 需要访问 Google Gemini API 和网易云音乐 API

## 📝 许可证

本项目采用 MIT 许可证。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

### 贡献指南
1. Fork 本项目
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 开启 Pull Request

### 开发环境设置
```bash
# 克隆项目
git clone <repository-url>
cd lyrics

# 安装依赖
./install-deps.sh

# 设置 API 密钥
export GEMINI_API_KEY="your_api_key_here"

# 编译项目
make all

# 安装到用户目录 (推荐)
make install-user

# 运行测试
make test  # 如果有测试的话
```

## 📞 支持

如果您遇到问题或有建议，请：
1. 查看 [常见问题](#常见问题) 部分
2. 检查现有的 [Issues](../../issues)
3. 创建新的 Issue 并详细描述您的问题

### 报告 Bug
请在 Issue 中包含以下信息：
- 操作系统和版本
- Hyprland 版本
- 错误日志或截图
- 重现步骤

### 功能请求
欢迎提出新功能建议！请描述：
- 功能的用途和价值
- 预期的行为
- 可能的实现方案

---

**享受您的智能歌词体验！** 🎵

*如果这个项目对您有帮助，请考虑给个 ⭐ Star！*
