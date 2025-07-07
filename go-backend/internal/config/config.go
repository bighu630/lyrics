package config

import (
	"log"
	"os"
	"path/filepath"
	"time"
)

const (
	DefaultSocketPath    = "/tmp/lyrics_app.sock"
	DefaultCheckInterval = 5 * time.Second
)

func getDefaultCacheDir() string {
	// 优先使用 XDG_CACHE_HOME 环境变量
	if cacheHome := os.Getenv("XDG_CACHE_HOME"); cacheHome != "" {
		return filepath.Join(cacheHome, "lyrics")
	}

	// 否则使用用户主目录下的 .cache
	homeDir, err := os.UserHomeDir()
	if err != nil {
		// 如果获取不到用户主目录，回退到当前目录
		return "lyrics_cache"
	}

	return filepath.Join(homeDir, ".cache", "lyrics")
}

type Config struct {
	SocketPath    string
	CheckInterval time.Duration
	CacheDir      string
	GeminiAPIKey  string
}

func Load() *Config {
	geminiAPIKey := os.Getenv("GEMINI_API_KEY")
	if geminiAPIKey == "" {
		log.Println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
		log.Println("!!! WARNING: GEMINI_API_KEY environment variable not set.     !!!")
		log.Println("!!! The application will not be able to fetch lyrics.         !!!")
		log.Println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
	}

	// 允许通过环境变量覆盖缓存目录
	cacheDir := os.Getenv("LYRICS_CACHE_DIR")
	if cacheDir == "" {
		cacheDir = getDefaultCacheDir()
	}

	return &Config{
		SocketPath:    DefaultSocketPath,
		CheckInterval: DefaultCheckInterval,
		CacheDir:      cacheDir,
		GeminiAPIKey:  geminiAPIKey,
	}
}
