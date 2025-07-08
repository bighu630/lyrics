# LRCLib 歌词源集成说明

## 概述

为了提供更高质量、更准确的歌词匹配，我们将 [LRCLib](https://lrclib.net) 作为最高优先级的歌词源集成到智能歌词显示系统中。本文档描述了实现细节、匹配算法以及系统架构的变更。

## 主要特性

1. **LRCLib 作为最高优先级歌词源**
   - 在查询歌词时优先使用 LRCLib API
   - 当 LRCLib 无法提供歌词时自动回退到网易云音乐等其他源
   - 相比国内音乐平台，LRCLib 对国际音乐有更好的支持

2. **智能歌词匹配算法**
   - 多级匹配策略：完全匹配（标题+艺术家）> 标题匹配 > 其他匹配
   - 歌曲时长匹配：在匹配结果中选择时长误差在阈值范围内（3秒）的歌词
   - 当无法找到精确匹配时，选择最接近的结果

3. **音乐信息传递增强**
   - 完整传递歌曲信息（包括时长）到歌词搜索流程
   - 从播放器（MPC/playerctl）获取歌曲时长
   - 基于时长的精确歌词匹配，减少误匹配率

## 实现细节

### LRCLib 客户端

**文件位置**: `pkg/lrclib/client.go`

```go
// Client LRCLib客户端
type Client struct {
    httpClient     *http.Client
    baseURL        string
    requestTimeout time.Duration
    maxRetries     int
}

// LRCLibResponse LRCLib API响应结构
type LRCLibResponse struct {
    ID           int    `json:"id"`
    Name         string `json:"name"`
    TrackName    string `json:"trackName"`
    ArtistName   string `json:"artistName"`
    AlbumName    string `json:"albumName"`
    Duration     int    `json:"duration"`
    Instrumental bool   `json:"instrumental"`
    PlainLyrics  string `json:"plainLyrics"`
    SyncedLyrics string `json:"syncedLyrics"`
}
```

### 智能匹配算法

```go
// findBestMatch 从搜索结果中找到最佳匹配的歌词
func (c *Client) findBestMatch(responses LRCLibSearchResponse, targetTitle, targetArtist string, targetDuration int) *LRCLibResponse {
    // 1. 按匹配精确度分类
    var exactMatches []*LRCLibResponse
    var titleMatches []*LRCLibResponse
    
    // 2. 筛选完全匹配和标题匹配
    for i := range responses {
        response := &responses[i]
        if containsIgnoreCase(response.TrackName, targetTitle) && 
           containsIgnoreCase(response.ArtistName, targetArtist) {
            exactMatches = append(exactMatches, response)
        } else if containsIgnoreCase(response.TrackName, targetTitle) {
            titleMatches = append(titleMatches, response)
        }
    }
    
    // 3. 选择匹配池
    matchPool := exactMatches
    if len(matchPool) == 0 {
        matchPool = titleMatches
    }
    if len(matchPool) == 0 {
        matchPool = make([]*LRCLibResponse, len(responses))
        for i := range responses {
            matchPool[i] = &responses[i]
        }
    }
    
    // 4. 在匹配池中筛选时长最接近的
    if targetDuration > 0 && len(matchPool) > 0 {
        const maxDurationDiff = 3 // 最大允许3秒误差
        bestMatch := matchPool[0]
        minDiff := abs(bestMatch.Duration - targetDuration)
        
        for _, m := range matchPool {
            diff := abs(m.Duration - targetDuration)
            if diff < minDiff {
                minDiff = diff
                bestMatch = m
            }
            
            // 如果误差在允许范围内，立即返回
            if diff <= maxDurationDiff {
                return m
            }
        }
        
        // 返回时长最接近的
        return bestMatch
    }
    
    // 5. 返回匹配结果或默认第一个
    if len(matchPool) > 0 {
        return matchPool[0]
    }
    return &responses[0]
}
```

## 音乐API架构变更

1. **音乐API接口扩展**
   - 扩展了 `MusicAPI` 和 `MusicManager` 接口以支持传递歌曲时长
   - `GetLyricsByInfo` 方法现在接受歌曲时长参数

2. **音乐提供商注册顺序**
   - 更新了 `factory.go` 中的提供商注册顺序
   - LRCLib > 网易云音乐 > QQ音乐（未完全实现）

3. **播放器时长获取增强**
   - `player.go` 支持从 MPC 和 playerctl 获取歌曲总时长
   - `GetCurrentDuration()` 方法在 `lyrics.go` 中被调用，为歌词搜索提供时长信息

## 改进效果

1. **歌词质量提升**
   - 提高了歌词匹配准确率，特别是对于多个版本同名歌曲
   - 减少了由于时长不匹配导致的错误歌词同步问题

2. **适应性增强**
   - 系统能够更好地适应不同的音乐平台和播放器
   - 多级回退机制确保在一个源失败后能切换到其他源

3. **扩展性**
   - 系统架构允许轻松添加更多歌词源
   - 统一的接口让集成新的API变得简单

## 后续计划

1. 完善 QQ音乐 API 客户端实现
2. 考虑集成更多歌词源（如Musixmatch等）
3. 为匹配算法添加更多指标，如专辑名匹配、发行年份匹配等
4. 实现本地缓存优化，减少API请求次数
5. 添加用户偏好设置，允许用户选择默认歌词源优先级
