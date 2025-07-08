# 音乐API重构说明

## 概述

这次重构将网易云音乐API代码从`lyrics`包中提取到了独立的`pkg`包中，并创建了一个通用的音乐API接口，为未来集成更多音乐平台（如QQ音乐、酷狗音乐等）做好了准备。

## 新的架构设计

### 1. 通用音乐API接口

**文件位置**: `pkg/music/interface.go`

```go
type MusicAPI interface {
    SearchSong(ctx context.Context, title, artist string) (string, error)
    GetLyrics(ctx context.Context, songID string) (string, error)
    GetProviderName() string
}
```

### 2. 音乐提供商管理器

**文件位置**: `pkg/music/manager.go`

`Manager`类负责管理多个音乐提供商，支持：
- 多提供商回退机制
- 自动故障转移
- 统一的接口调用

### 3. 网易云音乐实现

**文件位置**: `pkg/netease/client.go`

完整的网易云音乐API实现，包括：
- 歌曲搜索功能
- 歌词获取功能
- 原文和翻译歌词合并
- 智能歌曲匹配算法

### 4. LRCLib 歌词源

**文件位置**: `pkg/lrclib/client.go`

实现LRCLib歌词API，提供高质量LRC歌词，包含：
- 完整的API客户端实现
- 智能匹配算法
- 基于时长的歌词筛选
- 多级别匹配策略

### 5. QQ音乐模板

**文件位置**: `pkg/qqmusic/client.go`

为未来实现QQ音乐API提供的代码模板，包含：
- 基础结构定义
- 接口实现框架
- TODO注释指导

## 主要改进

### 1. 可扩展性
- ✅ 支持多个音乐提供商
- ✅ 统一的接口设计
- ✅ 插件式架构

### 2. 可靠性
- ✅ 自动故障转移
- ✅ 多提供商回退
- ✅ 详细的日志记录

### 3. 精确匹配
- ✅ 基于时长的歌词匹配
- ✅ 智能匹配算法
- ✅ 多级别匹配策略

### 3. 代码组织
- ✅ 职责分离
- ✅ 模块化设计
- ✅ 清晰的依赖关系

## 使用方法

### 1. 基本使用（自动管理）

```go
// 创建Provider时会自动初始化音乐管理器
provider, err := lyrics.NewProvider(cacheDir, geminiAPIKey)
if err != nil {
    log.Fatal(err)
}

// 正常使用，内部会自动选择最佳的音乐提供商
lyrics, err := provider.GetLyrics(ctx, "歌曲标题")
```

### 2. 手动创建音乐管理器

```go
// 创建默认管理器（包含所有可用的提供商）
manager, err := music.CreateDefaultManager()

// 或手动指定提供商
netease, _ := music.CreateProvider(music.ProviderNetEase)
qqmusic, _ := music.CreateProvider(music.ProviderQQMusic)
manager := music.NewManager([]music.MusicAPI{netease, qqmusic})

// 搜索歌曲
songID, err := manager.SearchSong(ctx, "歌曲名", "歌手名")

// 获取歌词
lyrics, err := manager.GetLyrics(ctx, songID)
```

## 添加新的音乐提供商

### 1. 创建新的客户端包

```bash
mkdir -p pkg/newprovider
```

### 2. 实现MusicAPI接口

```go
package newprovider

type Client struct {
    // 客户端字段
}

func NewClient() *Client {
    return &Client{}
}

func (c *Client) SearchSong(ctx context.Context, title, artist string) (string, error) {
    // 实现搜索逻辑
}

func (c *Client) GetLyrics(ctx context.Context, songID string) (string, error) {
    // 实现歌词获取逻辑
}

func (c *Client) GetProviderName() string {
    return "New Provider Name"
}
```

### 3. 更新工厂函数

在`pkg/music/factory.go`中添加新的提供商：

```go
case ProviderNewProvider:
    return newprovider.NewClient(), nil
```

### 4. 更新Provider枚举

在`pkg/music/interface.go`中添加：

```go
const (
    ProviderNewProvider Provider = "newprovider"
)
```

## 配置说明

### 环境变量

- `NETEASE_COOKIE`: 网易云音乐Cookie（可选）
- `QQMUSIC_COOKIE`: QQ音乐Cookie（未来使用）

### 提供商优先级

当前优先级（可在`CreateDefaultManager`中修改）：

1. **网易云音乐** - 主要提供商，API稳定
2. **QQ音乐** - 备选提供商（开发中）
3. **其他提供商** - 未来扩展

## 测试

### 1. 单元测试

```bash
cd go-backend
go test ./pkg/netease/...
go test ./pkg/music/...
```

### 2. 集成测试

```bash
# 启动后端测试
make backend
./build/lyrics-backend &

# 检查日志输出
tail -f lyrics.log
```

## 故障排除

### 1. 提供商不可用

日志会显示：
```
WARN: Provider NetEase Cloud Music failed: connection error
INFO: Trying provider QQ Music (2/2)
```

解决方案：
- 检查网络连接
- 验证API密钥
- 查看提供商服务状态

### 2. 所有提供商失败

错误信息：
```
ERROR: all providers failed, last error: network timeout
```

解决方案：
- 检查网络配置
- 验证防火墙设置
- 等待服务恢复

## 开发计划

### Phase 1 ✅ 已完成
- [x] 重构网易云音乐API
- [x] 创建通用音乐接口
- [x] 实现多提供商管理器
- [x] 集成到现有系统

### Phase 2 ✅ 已完成
- [x] 集成 LRCLib 歌词源
- [x] 实现智能匹配算法
- [x] 添加歌曲时长支持
- [x] 优化歌词匹配准确率

### Phase 3 🚧 开发中
- [ ] 完整实现QQ音乐API
- [ ] 添加更多错误处理
- [ ] 性能优化

### Phase 4 📋 计划中
- [ ] 酷狗音乐API
- [ ] 本地歌词文件支持
- [ ] 歌词质量评分系统
- [ ] 缓存策略优化

## 贡献指南

1. **添加新提供商**: 参考`pkg/qqmusic/client.go`模板
2. **改进现有功能**: 关注错误处理和性能
3. **测试**: 确保所有功能正常工作
4. **文档**: 更新相关文档和注释

---

**总结**: 这次重构显著提升了系统的可扩展性和可维护性，为未来支持更多音乐平台奠定了坚实的基础。通过统一的接口设计和智能的故障转移机制，用户可以享受到更稳定可靠的歌词获取服务。
