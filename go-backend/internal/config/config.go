package config

import (
	"bufio"
	"log"
	"os"
	"path/filepath"
	"strings"
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

// loadEnvFiles 尝试加载环境变量配置文件
func loadEnvFiles() {
	// 获取当前工作目录
	wd, _ := os.Getwd()

	// 按优先级顺序尝试加载配置文件
	envFiles := []string{
		".env",                    // 当前目录的 .env 文件
		".env.local",              // 当前目录的本地环境配置
		filepath.Join(wd, ".env"), // 工作目录的 .env 文件
		filepath.Join(os.Getenv("HOME"), ".lyrics.env"), // 用户主目录配置
	}

	for _, envFile := range envFiles {
		if _, err := os.Stat(envFile); err == nil {
			log.Printf("Loading environment variables from: %s", envFile)
			loadEnvFile(envFile)
			break // 只加载第一个找到的文件
		}
	}
}

// loadEnvFile 加载指定的环境变量文件
func loadEnvFile(filename string) {
	file, err := os.Open(filename)
	if err != nil {
		return
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())

		// 跳过空行和注释行
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		// 解析 KEY=VALUE 格式
		if parts := strings.SplitN(line, "=", 2); len(parts) == 2 {
			key := strings.TrimSpace(parts[0])
			value := strings.TrimSpace(parts[1])

			// 移除值两边的引号（如果有）
			if len(value) >= 2 &&
				((value[0] == '"' && value[len(value)-1] == '"') ||
					(value[0] == '\'' && value[len(value)-1] == '\'')) {
				value = value[1 : len(value)-1]
			}

			// 只有在环境变量未设置时才设置
			if os.Getenv(key) == "" {
				os.Setenv(key, value)
			}
		}
	}
}

type Config struct {
	SocketPath    string
	CheckInterval time.Duration
	CacheDir      string
	GeminiAPIKey  string
}

func Load() *Config {
	// 首先尝试加载环境变量配置文件
	loadEnvFiles()

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
