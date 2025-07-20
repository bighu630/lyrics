package music

import (
	"fmt"
	"go-backend/internal/config"
	"go-backend/pkg/lrclib"
	"go-backend/pkg/netease"
	"go-backend/pkg/qqmusic"

	"github.com/rs/zerolog/log"
)

// CreateProvider 创建音乐提供商客户端
func CreateProvider(provider Provider, lrcCfg config.LrcProviderConfig) (MusicAPI, error) {
	switch provider {
	case ProviderLRCLib:
		log.Info().Str("provider", "lrclib").Msg("Creating LRCLib client")
		return lrclib.NewClient(), nil
	case ProviderNetEase:
		log.Info().Str("provider", "netease").Msg("Creating NetEase music client")
		return netease.NewClient(lrcCfg.NeteaseCookie), nil
	case ProviderQQMusic:
		log.Info().Str("provider", "qqmusic").Msg("Creating QQ Music client")
		return qqmusic.NewClient(), nil
	case ProviderKugou:
		return nil, fmt.Errorf("kugou music provider not implemented yet")
	default:
		return nil, fmt.Errorf("unknown music provider: %s", provider)
	}
}

// CreateDefaultManager 创建默认的音乐API管理器
func CreateDefaultManager(lrcCfg config.LrcProviderConfig) (*Manager, error) {
	var providers []MusicAPI

	// 按优先级添加提供商
	providerTypes := []Provider{
		ProviderLRCLib,  // LRCLib作为最高优先级提供商
		ProviderNetEase, // 网易云音乐作为次要提供商
		// ProviderQQMusic, // QQ音乐作为备选（目前未完全实现）
	}

	for _, providerType := range providerTypes {
		provider, err := CreateProvider(providerType, lrcCfg)
		if err != nil {
			log.Warn().
				Str("provider", string(providerType)).
				Err(err).
				Msg("Failed to create provider")
			continue
		}
		providers = append(providers, provider)
	}

	if len(providers) == 0 {
		return nil, fmt.Errorf("no music providers available")
	}

	log.Info().
		Int("provider_count", len(providers)).
		Msg("Created music API manager")

	return NewManager(providers), nil
}

// GetAvailableProviders 获取所有可用的提供商
func GetAvailableProviders() []Provider {
	return []Provider{
		ProviderNetEase,
		ProviderQQMusic, // 已创建模板，但未完全实现
		// ProviderKugou,   // 未来实现
	}
}

// GetProviderByName 根据名称获取提供商
func GetProviderByName(name string) (Provider, error) {
	switch name {
	case "netease", "网易云", "163":
		return ProviderNetEase, nil
	case "qqmusic", "qq", "腾讯":
		return ProviderQQMusic, nil
	case "kugou", "酷狗":
		return ProviderKugou, nil
	default:
		return "", fmt.Errorf("unknown provider name: %s", name)
	}
}
