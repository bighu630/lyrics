package music

import (
	"context"
	"fmt"
	"go-backend/pkg/lrclib"

	"github.com/rs/zerolog/log"
)

// Provider 音乐提供商类型
type Provider string

const (
	// ProviderLRCLib LRCLib歌词库
	ProviderLRCLib Provider = "lrclib"
	// ProviderNetEase 网易云音乐
	ProviderNetEase Provider = "netease"
	// ProviderQQMusic QQ音乐 (未来实现)
	ProviderQQMusic Provider = "qqmusic"
	// ProviderKugou 酷狗音乐 (未来实现)
	ProviderKugou Provider = "kugou"
)

var logger = log.With().Str("component", "music-manager").Logger()

// Manager 音乐API管理器
type Manager struct {
	providers []MusicAPI
	primary   MusicAPI
}

// NewManager 创建新的音乐API管理器
func NewManager(providers []MusicAPI) *Manager {
	if len(providers) == 0 {
		logger.Warn().Msg("No music providers configured")
		return &Manager{}
	}

	primary := providers[0]
	logger.Info().
		Int("provider_count", len(providers)).
		Str("primary_provider", primary.GetProviderName()).
		Msg("Music API Manager initialized")

	return &Manager{
		providers: providers,
		primary:   primary,
	}
}

// SearchSong 搜索歌曲，支持多提供商回退
func (m *Manager) SearchSong(ctx context.Context, title, artist string) (string, error) {
	if len(m.providers) == 0 {
		return "", fmt.Errorf("no music providers available")
	}

	var lastErr error
	for i, provider := range m.providers {
		logger.Info().
			Str("provider", provider.GetProviderName()).
			Int("attempt", i+1).
			Int("total_providers", len(m.providers)).
			Msg("Trying provider")

		songID, err := provider.SearchSong(ctx, title, artist)
		if err == nil {
			logger.Info().
				Str("provider", provider.GetProviderName()).
				Msg("Successfully found song")
			return songID, nil
		}

		logger.Warn().
			Str("provider", provider.GetProviderName()).
			Err(err).
			Msg("Provider failed")
		lastErr = err
	}

	return "", fmt.Errorf("all providers failed, last error: %w", lastErr)
}

// GetLyrics 获取歌词，支持多提供商回退
func (m *Manager) GetLyrics(ctx context.Context, songID string) (string, error) {
	if len(m.providers) == 0 {
		return "", fmt.Errorf("no music providers available")
	}

	var lastErr error
	for i, provider := range m.providers {
		logger.Info().
			Str("provider", provider.GetProviderName()).
			Int("attempt", i+1).
			Int("total_providers", len(m.providers)).
			Msg("Trying to get lyrics from provider")

		lyrics, err := provider.GetLyrics(ctx, songID)
		if err == nil {
			logger.Info().
				Str("provider", provider.GetProviderName()).
				Msg("Successfully got lyrics")
			return lyrics, nil
		}

		logger.Warn().
			Str("provider", provider.GetProviderName()).
			Err(err).
			Msg("Provider failed")
		lastErr = err
	}

	return "", fmt.Errorf("all providers failed, last error: %w", lastErr)
}

// GetLyricsByInfo 根据歌曲信息直接获取歌词（封装搜索+获取歌词）
func (m *Manager) GetLyricsByInfo(ctx context.Context, title, artist string, duration float64) (string, error) {
	if len(m.providers) == 0 {
		return "", fmt.Errorf("no music providers available")
	}

	var lastErr error
	for i, provider := range m.providers {
		logger.Info().
			Str("title", title).
			Str("artist", artist).
			Float64("duration", duration).
			Str("provider", provider.GetProviderName()).
			Int("attempt", i+1).
			Int("total_providers", len(m.providers)).
			Msg("Trying to get lyrics")

		// 检查是否为LRCLib客户端并支持带时长的歌词查询
		if lrcClient, ok := provider.(*lrclib.Client); ok && duration > 0 {
			lyrics, err := lrcClient.GetLyricsByInfo(ctx, title, artist, duration)
			if err == nil {
				logger.Info().Msg("Successfully got lyrics from LRCLib using duration")
				return lyrics, nil
			}
			logger.Warn().Err(err).Msg("LRCLib with duration failed")
			lastErr = err
			continue
		}

		// 普通API流程：搜索歌曲
		songID, err := provider.SearchSong(ctx, title, artist)
		if err != nil {
			logger.Warn().
				Str("provider", provider.GetProviderName()).
				Err(err).
				Msg("Provider search failed")
			lastErr = err
			continue
		}

		// 获取歌词
		lyrics, err := provider.GetLyrics(ctx, songID)
		if err != nil {
			logger.Warn().
				Str("provider", provider.GetProviderName()).
				Str("song_id", songID).
				Err(err).
				Msg("Provider get lyrics failed")
			lastErr = err
			continue
		}

		logger.Info().
			Str("title", title).
			Str("artist", artist).
			Str("provider", provider.GetProviderName()).
			Msg("Successfully got lyrics")
		return lyrics, nil
	}

	return "", fmt.Errorf("all providers failed to get lyrics for '%s - %s', last error: %w", title, artist, lastErr)
}

// GetProviderName 获取管理器名称（实现MusicAPI接口）
func (m *Manager) GetProviderName() string {
	if m.primary != nil {
		return fmt.Sprintf("Manager[Primary: %s]", m.primary.GetProviderName())
	}
	return "Manager[No Providers]"
}

// GetPrimaryProvider 获取主提供商
func (m *Manager) GetPrimaryProvider() MusicAPI {
	return m.primary
}

// GetProviderCount 获取提供商数量
func (m *Manager) GetProviderCount() int {
	return len(m.providers)
}

// GetProviderNames 获取所有提供商名称
func (m *Manager) GetProviderNames() []string {
	names := make([]string, len(m.providers))
	for i, provider := range m.providers {
		names[i] = provider.GetProviderName()
	}
	return names
}
