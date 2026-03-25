# Lyrics - 桌面歌词

Linux (Hyprland/Wayland) 下的实时桌面歌词显示工具。通过 MPRIS 监听播放器，AI 识别歌曲，自动获取并同步显示歌词。

![image](https://github.com/user-attachments/assets/84d96bf3-fcd6-48f1-8cd7-864d0003861a)

## 快速开始

### 1. 安装依赖

```bash
# Arch Linux
sudo pacman -S go gtk4 pkg-config playerctl socat

# Ubuntu/Debian
sudo apt install golang-go libgtk-4-dev pkg-config playerctl socat

# Fedora
sudo dnf install golang gtk4-devel pkgconf-pkg-config playerctl socat
```

### 2. 配置

```bash
mkdir -p ~/.config/lyrics
cp config.toml.example ~/.config/lyrics/config.toml
```

编辑 `~/.config/lyrics/config.toml`，填入 AI API 密钥：

```toml
[ai]
module_name = "gemini"                # 或 "openai"
api_key = "your_api_key_here"         # Gemini: https://makersuite.google.com/app/apikey
# base_url = ""                       # 仅 OpenAI 兼容 API 需要
```

### 3. 编译 & 运行

```bash
make all                # 编译
make run-backend &      # 启动后端
make run-gui            # 启动歌词窗口 (或 make run-gui-console 调试模式)
```

### 4. 安装到系统 (可选)

```bash
make install-user       # 安装到 ~/.local/bin/
# 之后可以直接运行：
lyrics-backend &
lyrics-gui
```

## Hyprland 配置

在 `~/.config/hypr/hyprland.conf` 中添加：

```bash
# 窗口规则
windowrulev2 = float, class:^(lyrics-gui)$
windowrulev2 = pin, class:^(lyrics-gui)$
windowrulev2 = noborder, class:^(lyrics-gui)$
windowrulev2 = noshadow, class:^(lyrics-gui)$
windowrulev2 = noblur, class:^(lyrics-gui)$
windowrulev2 = move 10% 80%, class:^(lyrics-gui)$
windowrulev2 = size 80% 120, class:^(lyrics-gui)$

# 自动启动
exec-once = lyrics-backend
exec-once = sleep 2 && lyrics-gui

# 快捷键 (可选)
bind = SUPER, L, exec, pkill lyrics-gui || lyrics-gui
bind = SUPER SHIFT, L, exec, pkill lyrics-backend && sleep 1 && lyrics-backend
```

## Waybar 集成

```bash
cp waybar_lyrics.sh ~/.local/bin/ && chmod +x ~/.local/bin/waybar_lyrics.sh
```

在 `~/.config/waybar/config` 中添加模块：

```json
"custom/lyrics": {
    "format": "{}",
    "exec": "~/.local/bin/waybar_lyrics.sh",
    "interval": 1,
    "on-click": "lyrics-gui",
    "tooltip": true,
    "max-length": 60
}
```

## 工作原理

```
播放器 (Spotify/Chrome/VLC)
  → playerctl (MPRIS)
    → AI 识别歌曲 (Gemini/OpenAI)
      → 获取歌词 (LRCLib/网易云)
        → IPC 推送 → GTK4 透明窗口显示
```

歌词会缓存到 `~/.cache/lyrics/`，避免重复下载。

## 常见问题

| 问题 | 排查 |
|------|------|
| 窗口不显示 | `pkg-config --modversion gtk4` 检查 GTK4 |
| 连不上后端 | `ls /tmp/lyrics_app.sock` 检查 socket |
| 没有歌词 | `playerctl metadata title` 检查播放器 |
| AI 不响应 | 检查 config.toml 中的 api_key |

## 相关文档

- [快速开始指南](QUICK_START.md)
- [GUI 详细说明](README-GUI.md)
- [音乐API重构说明](MUSIC_API_REFACTORING.md)

## License

MIT
