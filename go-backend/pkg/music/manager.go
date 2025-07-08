package music

import (
	"context"
	"fmt"
	"log"
)

// Provider 音乐提供商类型
type Provider string

const (
	// ProviderNetEase 网易云音乐
	ProviderNetEase Provider = "netease"
	// ProviderQQMusic QQ音乐 (未来实现)
	ProviderQQMusic Provider = "qqmusic"
	// ProviderKugou 酷狗音乐 (未来实现)
	ProviderKugou Provider = "kugou"
)

// Manager 音乐API管理器
type Manager struct {
	providers []MusicAPI
	primary   MusicAPI
}

// NewManager 创建新的音乐API管理器
func NewManager(providers []MusicAPI) *Manager {
	if len(providers) == 0 {
		log.Printf("WARN: No music providers configured")
		return &Manager{}
	}

	primary := providers[0]
	log.Printf("INFO: Music API Manager initialized with %d providers, primary: %s", len(providers), primary.GetProviderName())

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
		log.Printf("INFO: Trying provider %s (%d/%d)", provider.GetProviderName(), i+1, len(m.providers))

		songID, err := provider.SearchSong(ctx, title, artist)
		if err == nil {
			log.Printf("INFO: Successfully found song with provider %s", provider.GetProviderName())
			return songID, nil
		}

		log.Printf("WARN: Provider %s failed: %v", provider.GetProviderName(), err)
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
		log.Printf("INFO: Trying to get lyrics from provider %s (%d/%d)", provider.GetProviderName(), i+1, len(m.providers))

		lyrics, err := provider.GetLyrics(ctx, songID)
		if err == nil {
			log.Printf("INFO: Successfully got lyrics from provider %s", provider.GetProviderName())
			return lyrics, nil
		}

		log.Printf("WARN: Provider %s failed: %v", provider.GetProviderName(), err)
		lastErr = err
	}

	return "", fmt.Errorf("all providers failed, last error: %w", lastErr)
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
