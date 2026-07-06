# GUI 模式说明

> ⚠️ **该项目已重构**，旧文档仅供参考。请查看 [README.md](README.md) 获取最新信息。

## 显示模式

项目现为单一二进制 `lyrics`，支持三种模式：

| 模式 | 命令 | 说明 |
|------|------|------|
| **GTK4 透明窗口** | `lyrics` 或 `lyrics`（默认） | 无边框透明背景，青绿色发光文字 |
| **终端 TUI** | `lyrics --console` | 居中终端显示 |
| **无头模式** | `lyrics --no-gui` | 仅写入 /dev/shm/lyrics |

详细说明请查看 [README.md](README.md)。
