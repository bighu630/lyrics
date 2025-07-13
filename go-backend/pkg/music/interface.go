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

// MusicManager 音乐管理器接口（扩展接口，包含组合操作）
type MusicManager interface {
	MusicAPI

	// GetLyricsByInfo 根据歌曲信息直接获取歌词（封装搜索+获取歌词）
	GetLyricsByInfo(ctx context.Context, title, artist string, duration float64) (string, error)
}

// LyricLine 歌词行结构
type LyricLine struct {
	Time float64 // 时间戳（秒）
	Text string  // 歌词文本
}

// SongInfo 歌曲信息结构
type SongInfo struct {
	Title    string  `json:"title"`
	Artist   string  `json:"artist"`
	Duration float64 `json:"duration"` // 歌曲时长（秒）
	IsSong   bool    `json:"is_song"`
}
