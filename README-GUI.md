# Lyrics Application - GUI + Console Mode

这个应用程序支持两种显示模式：控制台模式和透明窗口 GUI 模式。

## 安装依赖

首先安装必要的依赖：

```bash
./install-deps.sh
```

## 编译

```bash
# 编译所有组件
make all

# 只编译后端
make backend

# 只编译GUI客户端
make gui
```

## 运行

### 1. 启动后端服务
```bash
make run-backend
# 或者
./build/lyrics-backend
```

### 2. 启动客户端

#### 控制台模式（默认）
```bash
make run-gui
# 或者
./build/lyrics-gui
```

#### 透明窗口 GUI 模式
```bash
make run-gui-window
# 或者
./build/lyrics-gui --gui
```

## 功能特性

### 控制台模式
- 歌词居中显示在终端
- 自动适应终端宽度
- 简洁的文本界面

### GUI 模式
- 透明背景窗口
- 白色文字带阴影效果
- 总是在最前显示
- 适合在 Hyprland 等窗口管理器下使用

### 通用特性
- 自动重连机制
- 服务端重启时客户端自动重连
- 可配置的重连延迟和次数
- 优雅的错误处理

## Hyprland 配置建议

为了在 Hyprland 中更好地使用 GUI 模式，可以添加以下窗口规则：

```bash
# ~/.config/hypr/hyprland.conf

# 歌词窗口规则
windowrulev2 = float,class:^(lyrics)$
windowrulev2 = pin,class:^(lyrics)$
windowrulev2 = noborder,class:^(lyrics)$
windowrulev2 = noshadow,class:^(lyrics)$
windowrulev2 = move 10% 5%,class:^(lyrics)$
windowrulev2 = size 80% 150,class:^(lyrics)$
```

## 故障排除

### GTK 相关错误
如果遇到 GTK 相关错误，确保已安装 GTK4：
```bash
sudo apt install libgtk-4-dev libgtk-4-1
```

### 连接失败
1. 确保后端服务正在运行
2. 检查 socket 文件权限：`ls -la /tmp/lyrics_app.sock`
3. 查看后端日志输出

### 透明窗口不工作
1. 确保合成器支持透明度
2. 在 Hyprland 中检查 `decoration:blur` 设置
3. 尝试调整窗口管理器配置

## 开发

### 架构说明
- **后端**: Go 语言，负责获取歌词和 IPC 通信
- **前端**: C++ + GTK4，支持控制台和 GUI 两种模式
- **IPC**: Unix Domain Socket 通信
- **输出抽象**: 使用多态接口支持不同显示方式

### 添加新的输出方式
实现 `LyricsOutput` 接口：
```cpp
class MyOutput : public LyricsOutput {
    // 实现所有虚函数
};
```
