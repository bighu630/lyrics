package music

import (
	"context"
)

// MusicAPI 音乐API通用接口
type MusicAPI interface {
	// SearchSong 搜索歌曲，返回歌曲ID
	SearchSong(ctx context.Context, title, artist string) (string, error)

	// GetLyrics 根据歌曲ID获取歌词
	GetLyrics(ctx context.Context, songID string) (string, error)

	// GetProviderName 获取音乐提供商名称
	GetProviderName() string
}

// LyricLine 歌词行结构
type LyricLine struct {
	Time float64 // 时间戳（秒）
	Text string  // 歌词文本
}
