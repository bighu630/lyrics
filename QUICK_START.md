# 快速开始 - 智能歌词显示系统

最简单的安装和使用指南，5分钟快速上手。

## 📦 一键安装

```bash
# 1. 安装依赖
./install-deps.sh

# 2. 编译项目
make all

# 3. 安装到用户目录
make install-user
```

## ⚙️ 配置文件

创建配置文件并设置 API 密钥：

```bash
# 创建配置目录
mkdir -p ~/.config/lyrics

# 创建配置文件
cat > ~/.config/lyrics/config.toml << 'EOF'
[app]
socket_path = "/tmp/lyrics_app.sock"
check_interval = "5s"
cache_dir = ""

[ai]
module_name = "gemini"
api_key = "your_gemini_api_key_here"
base_url = ""
EOF

# 编辑配置文件，替换 API 密钥
nano ~/.config/lyrics/config.toml
```

### 获取 Gemini API 密钥

1. 访问 https://makersuite.google.com/app/apikey
2. 登录 Google 账号
3. 点击 "Create API Key"
4. 复制 API 密钥到配置文件中

## 📱 Waybar 配置


在 `~/.config/waybar/config` 中添加：

```json
{
    "modules-center": ["custom/lyrics"],
    
    "custom/lyrics": {
        "format": "{}",
        "exec": "awk 'NR==1 {for(i=1;i<=length&&i<=30;i++) printf substr($0,i,1)}' /dev/shm/lyrics",
        "interval": 1,
        "on-click": "lyrics-gui",
        "tooltip": true
    }
}
```

在 `~/.config/waybar/style.css` 中添加样式：

```css
#custom-lyrics {
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
```

## 🚀 启动命令

```bash
# 启动后端服务（后台运行）
lyrics-backend &

# 启动歌词窗口
lyrics-gui

# 重启 Waybar（应用配置）
pkill waybar && waybar &
```

## 🔧 Hyprland 集成（可选）

在 `~/.config/hypr/hyprland.conf` 中添加：

```bash
# 窗口规则
windowrulev2 = float, class:^(lyrics-gui)$
windowrulev2 = pin, class:^(lyrics-gui)$
windowrulev2 = noborder, class:^(lyrics-gui)$

# 自动启动
exec-once = lyrics-backend
exec-once = sleep 2 && lyrics-gui

# 快捷键
bind = SUPER, L, exec, pkill lyrics-gui || lyrics-gui
```

## ✅ 验证安装

```bash
# 检查后端是否运行
ps aux | grep lyrics-backend

# 检查配置文件
cat ~/.config/lyrics/config.toml

# 测试 Waybar 脚本
~/.local/bin/waybar_lyrics.sh
```

## 🎵 开始使用

1. 打开音乐播放器（Spotify、Chrome 播放音乐等）
2. 播放歌曲
3. 歌词会自动显示在 Waybar 和透明窗口中

就这么简单！享受智能歌词体验 🎶

---

遇到问题？查看完整文档：[README.md](README.md)
