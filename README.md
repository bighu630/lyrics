<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" lang="zh-CN">
<head>
<meta charset="utf-8"/>
</head>
<body>

<!--
  README for Lyrics Display System
  This HTML renders beautifully on GitHub.
-->

<h1 align="center">
  🎶 Lyrics — 桌面智能歌词显示
</h1>

<p align="center">
  <strong>Linux 桌面歌词工具</strong> · 多模式 · AI 智能识别 · 多源获取 · 毫秒级同步
</p>

<p align="center">
  <img alt="GTK4 歌词窗口预览" src="https://github.com/user-attachments/assets/84d96bf3-fcd6-48f1-8cd7-864d0003861a" width="720"/>
</p>

<p align="center">
  <a href="#-features">特性</a> •
  <a href="#-quick-start">快速开始</a> •
  <a href="#-configuration">配置</a> •
  <a href="#-display-modes">显示模式</a> •
  <a href="#-window-manager-integration">WM 集成</a> •
  <a href="#-architecture">架构</a>
</p>

<hr/>

<blockquote>
  <strong>🚨 完全重构</strong> — 该项目已从 Go 后端 + C++ 前端双进程架构重构为<strong>单一 C++17 二进制文件</strong>，支持三种运行模式。旧版 README 已过时，请以此为准。
</blockquote>

<br/>

<h2 id="-features">✨ 特性</h2>

<ul>
  <li>🎯 <strong>AI 智能歌曲识别</strong> — 通过 Gemini / OpenAI 自动从媒体标题中提取歌曲名和艺人</li>
  <li>📡 <strong>多源歌词获取</strong> — LRCLib + 网易云音乐，自动切换</li>
  <li>⏱ <strong>毫秒级歌词同步</strong> — LRC 解析 + 精确调度器，支持暂停/恢复/重同步</li>
  <li>🖥 <strong>三种显示模式</strong> — GTK4 透明窗口 / 终端 TUI / 无头模式</li>
  <li>🔗 <strong>Waybar / i3blocks 集成</strong> — 通过 <code>/dev/shm/lyrics</code> 和信号刷新</li>
  <li>📦 <strong>一键缓存</strong> — AI 识别结果 + LRC 文件双重缓存，避免重复请求</li>
  <li>🎮 <strong>MPRIS 兼容</strong> — 支持所有 MPRIS 播放器（Spotify, Chrome, VLC, mpd 等）</li>
  <li>🪟 <strong>Windows 支持</strong> — 通过 SMTC (System Media Transport Controls) 实现</li>
</ul>

<br/>

<h2 id="-features-advanced">🔬 功能截图</h2>

<table>
  <tr>
    <td align="center"><strong>GTK4 透明窗口</strong></td>
    <td align="center"><strong>终端 TUI 模式</strong></td>
  </tr>
  <tr>
    <td><img alt="gui" src="https://github.com/user-attachments/assets/84d96bf3-fcd6-48f1-8cd7-864d0003861a" width="400"/></td>
    <td><img alt="tui" src="https://github.com/user-attachments/assets/84d96bf3-fcd6-48f1-8cd7-864d0003861a" width="400"/></td>
  </tr>
</table>

<br/>

<h2 id="-quick-start">🚀 快速开始</h2>

<h3>📋 前提条件</h3>

<table>
  <tr>
    <th>发行版</th>
    <th>安装命令</th>
  </tr>
  <tr>
    <td><strong>Arch Linux</strong></td>
    <td><code>sudo pacman -S cmake gcc pkg-config gtk4 libcurl dbus playerctl mpc</code></td>
  </tr>
  <tr>
    <td><strong>Ubuntu / Debian</strong></td>
    <td><code>sudo apt install cmake g++ pkg-config libgtk-4-dev libcurl4-openssl-dev libdbus-1-dev playerctl mpc</code></td>
  </tr>
  <tr>
    <td><strong>Fedora</strong></td>
    <td><code>sudo dnf install cmake gcc-c++ pkgconfig gtk4-devel libcurl-devel dbus-devel playerctl mpc</code></td>
  </tr>
</table>

<blockquote>
  💡 <strong>spdlog</strong> 是唯一可选系统依赖；未安装时会自动通过 CMake FetchContent 下载。
</blockquote>

<h3>📥 安装</h3>

<pre><code># 1. 克隆并进入项目
git clone https://github.com/your-username/lyrics.git
cd lyrics

# 2. 构建
make

# 3. （可选）安装到 ~/.local/bin/
make install-user
</code></pre>

<p>构建产物在 <code>build/lyrics</code>。</p>

<h3>⚙️ 配置</h3>

<pre><code># 创建配置目录
mkdir -p ~/.config/lyrics

# 复制配置模板
cp config.toml.example ~/.config/lyrics/config.toml
</code></pre>

<p>编辑 <code>~/.config/lyrics/config.toml</code>，填入 AI API 密钥：</p>

<pre><code>[ai]
module_name = "gemini"              # "gemini" 或 "openai"
api_key = "your_api_key_here"      # 在此填入你的 API 密钥
</code></pre>

<details>
<summary><strong>🔑 获取 API 密钥</strong></summary>
<br/>
<ul>
  <li><strong>Gemini</strong>：访问 <a href="https://makersuite.google.com/app/apikey">Google AI Studio</a> → 创建 API 密钥（免费额度充足）</li>
  <li><strong>OpenAI</strong>：访问 <a href="https://platform.openai.com/api-keys">OpenAI Platform</a> → 创建 API 密钥（付费）</li>
  <li><strong>自定义 OpenAI 兼容 API</strong>：设置 <code>base_url</code> 指向你的接口地址（如 <code>https://api.example.com/v1</code>）</li>
</ul>
</details>

<br/>

<h3>🎬 运行</h3>

<pre><code># 默认启动 GTK4 透明歌词窗口（推荐）
./build/lyrics

# 或：终端 TUI 模式
./build/lyrics --console

# 或：无头模式（仅后台，无窗口）
./build/lyrics --no-gui
</code></pre>

<blockquote>
  <strong>提示</strong>：默认模式下，如果编译时未包含 GTK4 支持，会自动回退到 TUI 模式。
</blockquote>

<p align="right">
  <sub>更多构建选项见 <code>make help</code></sub>
</p>

<br/>

<h2 id="-configuration">⚙️ 完整配置说明</h2>

<p>配置文件位置：<code>~/.config/lyrics/config.toml</code></p>

<pre><code>[app]
# 检测歌曲变化的间隔（如 "5s", "10s", "1m"）
check_interval = "5s"

# 歌词缓存目录（留空则使用默认路径 ~/.cache/lyrics）
cache_dir = ""

# 日志级别：trace, debug, info, warn, error, critical, off
log_level = "info"

# 是否启用 GTK GUI 模式（优先，但会被命令行选项覆盖）
gui = false

[lrc]
# 网易云音乐 Cookie（可选，用于获取更高成功率的歌词）
netease_cookie = ""

[ai]
# AI 模块： "gemini"（推荐）或 "openai"
module_name = "gemini"

# API 密钥
api_key = "your_api_key_here"

# OpenAI 兼容 API 的基础 URL（仅 module_name = "openai" 时需要）
base_url = ""

# 模型名称（可选，不填则使用默认值）
# Gemini 默认: gemini-2.0-flash-exp
# OpenAI 默认: gpt-3.5-turbo
model_name = ""
</code></pre>

<br/>

<h2 id="-display-modes">🖥️ 显示模式</h2>

<table>
  <tr>
    <th>模式</th>
    <th>命令</th>
    <th>说明</th>
  </tr>
  <tr>
    <td>🎨 <strong>GTK4 透明窗口</strong></td>
    <td><code>lyrics</code></td>
    <td>无边框透明背景，青绿色发光文字，支持右键菜单调整字体大小，适合 Hyprland / Wayland</td>
  </tr>
  <tr>
    <td>💻 <strong>终端 TUI</strong></td>
    <td><code>lyrics --console</code></td>
    <td>居中显示在终端，自动适应终端宽度，不需要图形环境</td>
  </tr>
  <tr>
    <td>🤖 <strong>无头模式</strong></td>
    <td><code>lyrics --no-gui</code></td>
    <td>不启动任何窗口，仅写入 <code>/dev/shm/lyrics</code> 并发送 i3block 信号，适合 Waybar 用户</td>
  </tr>
</table>

<br/>

<h2 id="-window-manager-integration">🪟 窗口管理器集成</h2>

<h3>Hyprland</h3>

<p>在 <code>~/.config/hypr/hyprland.conf</code> 中添加：</p>

<pre><code># 歌词窗口规则
windowrulev2 = float, class:^(lyrics)$
windowrulev2 = pin, class:^(lyrics)$
windowrulev2 = noborder, class:^(lyrics)$
windowrulev2 = noshadow, class:^(lyrics)$
windowrulev2 = noblur, class:^(lyrics)$
windowrulev2 = move 10% 80%, class:^(lyrics)$
windowrulev2 = size 80% 120, class:^(lyrics)$

# 自动启动
exec-once = lyrics
</code></pre>

<h3>Waybar</h3>

<p>在 <code>~/.config/waybar/config</code> 中添加模块：</p>

<pre><code class="language-json">"custom/lyrics": {
    "format": "{}",
    "exec": "head -c 40 /dev/shm/lyrics",
    "interval": 1,
    "on-click": "lyrics",
    "max-length": 60
}
</code></pre>

<p>然后使用 <code>lyrics --no-gui</code> 后台运行。</p>

<details>
<summary><strong>🎨 Waybar 样式示例</strong></summary>
<pre><code class="language-css">#custom-lyrics {
    background: linear-gradient(to right, rgba(51, 204, 255, 0.6), rgba(0, 255, 153, 0.7));
    color: white;
    padding: 4px 8px;
    font-size: 14px;
    border-radius: 10px;
    font-weight: bold;
    margin: 0 5px;
    min-width: 200px;
    text-align: center;
}
</code></pre>
</details>

<h3>i3blocks</h3>

<p>程序会自动检测 i3blocks 进程，并通过 <code>SIGRTMIN+21</code>（Signal 55）触发块刷新。歌词内容写入 <code>/dev/shm/lyrics</code>。</p>

<pre><code># i3blocks 配置示例
lyrics {
    command=cat /dev/shm/lyrics
    interval=persist
    signal=55
}
</code></pre>

<br/>

<h2 id="-architecture">🏗️ 架构</h2>

<h3>数据流</h3>

<pre><code>┌─────────────────────────────────────────────────────────────┐
│                     🎵 播放器                                │
│          (Spotify / Chrome / VLC / mpd 等)                   │
└───────────────┬─────────────────────────────────────────────┘
                │ playerctl / mpc (MPRIS)
                ▼
┌─────────────────────────────────────────────────────────────┐
│               App（主循环，每 5 秒检测）                       │
│                                                             │
│  ┌─────────────┐   ┌──────────────┐   ┌──────────────────┐  │
│  │ player::    │   │ LyricsProvider│   │   Scheduler      │  │
│  │ get_current │──▶│ + AI 识别     │──▶│ + 时间同步       │  │
│  │ _song()     │   │ + 多源获取    │   │ + 回调推送       │  │
│  └─────────────┘   └──────────────┘   └────────┬─────────┘  │
│                                                  │           │
└──────────────────────────────────────────────────┼───────────┘
                                                   │
                    ┌──────────────────────────────┼──────────┐
                    │          输出层               │          │
                    │                              ▼          │
                    │  ┌──────────┐ ┌────────┐ ┌───────────┐ │
                    │  │ GTK4 GUI │ │ TUI    │ │ /dev/shm/ │ │
                    │  │ 透明窗口  │ │ 终端    │ │ + signal  │ │
                    │  └──────────┘ └────────┘ └───────────┘ │
                    └─────────────────────────────────────────┘
</code></pre>

<h3>歌词获取流程</h3>

<pre><code>歌曲标识符 ("Artist - Title")
  │
  ▼
① AI 识别（Gemini / OpenAI）
  │  提取 {is_song, title, artist}
  │  ↳ 结果缓存到 ~/.cache/lyrics/music_cache.jsonl
  ▼
② 查询 LRC 文件缓存
  │  ~/.cache/lyrics/{title}-{artist}.lrc
  │  ↳ 命中则直接返回
  ▼
③ 多源歌词获取
  │  LRCLib (https://lrclib.net)
  │  └→ 网易云音乐（需 cookie）
  │  ↳ 成功则缓存并返回
  ▼
④ 回退：返回错误信息
</code></pre>

<h3>歌词同步调度器</h3>

<ul>
  <li>基于当前播放位置 + 流逝时间的推算</li>
  <li>二分查找当前歌词行（O(log n)）</li>
  <li>提前 0.1 秒显示</li>
  <li>每 5 秒与播放器时间重同步</li>
  <li>歌曲结束后自动停止</li>
  <li>检测暂停/恢复，无缝续播</li>
</ul>

<br/>

<h2 id="-build-options">🛠️ 构建选项</h2>

<h3>Make 目标</h3>

<table>
  <tr><th>命令</th><th>说明</th></tr>
  <tr><td><code>make</code></td><td>构建 Release 版二进制 → <code>build/lyrics</code></td></tr>
  <tr><td><code>make run</code></td><td>构建并运行（GUI 模式）</td></tr>
  <tr><td><code>make run-console</code></td><td>构建并运行（TUI 模式）</td></tr>
  <tr><td><code>make run-headless</code></td><td>构建并运行（无头模式）</td></tr>
  <tr><td><code>make install</code></td><td>安装到 <code>/usr/local/bin</code>（需要 sudo）</td></tr>
  <tr><td><code>make install-user</code></td><td>安装到 <code>~/.local/bin</code></td></tr>
  <tr><td><code>make clean</code></td><td>清理构建文件</td></tr>
  <tr><td><code>make help</code></td><td>显示帮助信息</td></tr>
</table>

<h3>CMake 选项</h3>

<table>
  <tr><th>选项</th><th>默认值</th><th>说明</th></tr>
  <tr><td><code>BUILD_TESTS</code></td><td>OFF</td><td>构建单元测试</td></tr>
  <tr><td><code>USE_SYSTEM_SPDLOG</code></td><td>OFF</td><td>使用系统安装的 spdlog（替代自动下载）</td></tr>
  <tr><td><code>USE_SYSTEM_YYJSON</code></td><td>OFF</td><td>使用系统 yyjson（替代 bundled）</td></tr>
  <tr><td><code>USE_SYSTEM_TOMLC99</code></td><td>OFF</td><td>使用系统 tomlc99（替代 bundled）</td></tr>
</table>

<br/>

<h2 id="-providers">🧠 AI 与歌词源</h2>

<h3>AI 模块</h3>

<table>
  <tr><th>模块</th><th>默认模型</th><th>特点</th></tr>
  <tr><td><strong>Gemini</strong>（推荐）</td><td><code>gemini-2.0-flash-exp</code></td><td>免费额度充足，响应快</td></tr>
  <tr><td><strong>OpenAI</strong></td><td><code>gpt-3.5-turbo</code></td><td>兼容所有 OpenAI 格式的 API（如 DeepSeek, Qwen, 自建等）</td></tr>
</table>

<h3>歌词源</h3>

<table>
  <tr><th>源</th><th>优先级</th><th>是否需要额外配置</th></tr>
  <tr><td><strong>LRCLib</strong></td><td>🥇 第一</td><td>无需配置，全球曲库丰富</td></tr>
  <tr><td><strong>网易云音乐</strong></td><td>🥈 第二</td><td>需填写 <code>netease_cookie</code>（中文曲库更全）</td></tr>
</table>

<br/>

<h2 id="-cache">💾 缓存机制</h2>

<p>缓存目录默认为 <code>~/.cache/lyrics</code>（可通过 <code>cache_dir</code> 配置），包含两类缓存：</p>

<table>
  <tr><th>缓存文件</th><th>内容</th><th>格式</th></tr>
  <tr><td><code>music_cache.jsonl</code></td><td>AI 识别结果（歌曲标识符 → 歌曲信息 JSON）</td><td>JSON Lines</td></tr>
  <tr><td><code>*.lrc</code></td><td>下载的歌词文件，命名如 <code>千里之外-周杰伦.lrc</code></td><td>LRC 文本</td></tr>
</table>

<p>AI 缓存仅在 is_song=true 时持久化，非歌曲条目会自动过滤。</p>

<br/>

<h2 id="-windows">🪟 Windows 支持</h2>

<p>Windows 版本通过 <strong>SMTC（System Media Transport Controls）</strong> 检测播放状态：</p>

<ul>
  <li><code>smtc-helper/</code> — C# .NET 8 控制台程序，监听系统媒体事件，通过 stdout 输出 JSON</li>
  <li><code>player_windows.cpp</code> — 读取 SMTC 输出，实现 <code>player.h</code> 接口</li>
  <li><code>player_linux.cpp</code> 中的 MPRIS/playerctl 代码不参与 Windows 编译</li>
  <li><code>i3block/controller.cpp</code> 在 Windows 上禁用</li>
</ul>

<p>构建 Windows 版本需要 Visual Studio 2022 和 .NET 8 SDK。</p>

<br/>

<h2 id="-troubleshooting">🔧 常见问题</h2>

<table>
  <tr>
    <th>问题</th>
    <th>排查思路</th>
  </tr>
  <tr>
    <td>窗口不显示</td>
    <td>检查 GTK4 是否安装：<code>pkg-config --modversion gtk4</code>。尝试 <code>lyrics --console</code></td>
  </tr>
  <tr>
    <td>没有歌词</td>
    <td>确认播放器正在播放：<code>playerctl metadata title</code> 或 <code>mpc current</code></td>
  </tr>
  <tr>
    <td>AI 不响应</td>
    <td>检查 <code>~/.config/lyrics/config.toml</code> 中的 <code>api_key</code>。查看日志：<code>tail -f /tmp/lyrics.log</code></td>
  </tr>
  <tr>
    <td>歌词不同步</td>
    <td>确认播放器已安装 <code>playerctl</code> 或 <code>mpc</code>。调度器每 5 秒会自动重同步</td>
  </tr>
  <tr>
    <td>网易云不返回歌词</td>
    <td>需要设置 <code>netease_cookie</code>。从浏览器开发者工具复制 Cookie 字符串</td>
  </tr>
  <tr>
    <td>编译失败</td>
    <td>确保安装了 C++17 编译器（GCC 9+ / Clang 10+）和 CMake 3.20+</td>
  </tr>
</table>

<br/>

<h2 id="-development">🧑‍💻 开发</h2>

<h3>项目结构</h3>

<pre><code>lyrics/
├── CMakeLists.txt              # CMake 构建配置
├── Makefile                    # 便捷 make 命令
├── config.toml.example         # 配置模板
├── src/
│   ├── main.cpp                # 入口 + 模式分发
│   ├── app.cpp / app.h         # 主循环 + 歌词获取协调
│   ├── config.cpp / config.h   # TOML 配置解析
│   ├── player.h                # 播放器检测接口
│   ├── player_linux.cpp        # Linux MPC + Playerctl 实现
│   ├── player_windows.cpp      # Windows SMTC 实现
│   ├── ai/
│   │   ├── ai_interface.h      # AI 抽象接口
│   │   ├── gemini.cpp/h        # Gemini API 客户端
│   │   └── openai.cpp/h        # OpenAI API 客户端
│   ├── lyrics/
│   │   ├── provider.cpp/h      # 歌词获取编排（AI → 缓存 → 多源）
│   │   ├── lrc_parser.cpp/h    # LRC 格式解析
│   │   └── scheduler.cpp/h     # 歌词时间同步调度器
│   ├── music/
│   │   ├── music_manager.cpp/h # 多源管理器（链式调用）
│   │   ├── lrclib.cpp/h        # LRCLib 客户端
│   │   └── netease.cpp/h       # 网易云音乐客户端
│   ├── cache/
│   │   └── cache.cpp/h         # AI + LRC 缓存管理
│   ├── ui/
│   │   ├── gui.h               # GUI 模式接口
│   │   ├── gtk_window.cpp      # GTK4 透明窗口实现
│   │   ├── console_tui.cpp/h   # 终端 TUI 实现
│   ├── i3block/
│   │   └── controller.cpp/h    # i3blocks 信号集成
│   └── util/
│       ├── exec.h              # shell 命令执行
│       ├── http.cpp/h          # libcurl HTTP 客户端（含重试）
│       ├── json.cpp/h          # yyjson RAII 包装
│       └── log.h               # spdlog 初始化 + 宏
├── smtc-helper/                # Windows SMTC C# helper
│   ├── Program.cs
│   └── smtc-helper.csproj
└── third_party/                # 三方库
    ├── yyjson/                 # JSON 库
    └── tomlc99/                # TOML 解析库
</code></pre>

<h3>技术栈</h3>

<table>
  <tr><th>组件</th><th>技术</th></tr>
  <tr><td>语言</td><td>C++17</td></tr>
  <tr><td>构建系统</td><td>CMake 3.20+</td></tr>
  <tr><td>GUI</td><td>GTK4（默认依赖）</td></tr>
  <tr><td>HTTP</td><td>libcurl + 自动重试</td></tr>
  <tr><td>JSON</td><td>yyjson (bundled)</td></tr>
  <tr><td>TOML</td><td>tomlc99 (bundled)</td></tr>
  <tr><td>日志</td><td>spdlog（自动下载或系统安装）</td></tr>
  <tr><td>播放器检测</td><td>playerctl (MPRIS) / mpc (MPD) / SMTC (Windows)</td></tr>
  <tr><td>AI</td><td>Gemini API / OpenAI API</td></tr>
  <tr><td>歌词源</td><td>LRCLib API / 网易云音乐 API</td></tr>
</table>

<br/>

<h2 id="-license">📄 License</h2>

<p>MIT License. 详见 <a href="LICENSE">LICENSE</a> 文件。</p>

<br/>

<hr/>

<p align="center">
  <sub>Built with ❤️ for Linux desktop music lovers</sub>
</p>

</body>
</html>
