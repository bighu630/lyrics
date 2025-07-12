package config

import (
	"log"
	"os"
	"path/filepath"
	"time"

	"github.com/BurntSushi/toml"
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

// TomlConfig TOML配置文件结构
type TomlConfig struct {
	App struct {
		SocketPath    string `toml:"socket_path"`
		CheckInterval string `toml:"check_interval"`
		CacheDir      string `toml:"cache_dir"`
	} `toml:"app"`

	AI struct {
		ModuleName string `toml:"module_name"`
		APIKey     string `toml:"api_key"`
		BaseURL    string `toml:"base_url"` // for OpenAI
	} `toml:"ai"`
}

type Config struct {
	SocketPath    string
	CheckInterval time.Duration
	CacheDir      string
	AiModuleName  string
	AiRuleURL     string
	AiAPIKey      string
}

// getConfigPath 获取配置文件路径
func getConfigPath() string {
	// 优先使用 XDG_CONFIG_HOME 环境变量
	if configHome := os.Getenv("XDG_CONFIG_HOME"); configHome != "" {
		return filepath.Join(configHome, "lyrics", "config.toml")
	}

	// 否则使用用户主目录下的 .config
	homeDir, err := os.UserHomeDir()
	if err != nil {
		log.Printf("WARN: Cannot get user home directory: %v", err)
		return "config.toml" // 回退到当前目录
	}

	return filepath.Join(homeDir, ".config", "lyrics", "config.toml")
}

// loadTomlConfig 加载TOML配置文件
func loadTomlConfig() (*TomlConfig, error) {
	configPath := getConfigPath()

	// 检查配置文件是否存在
	if _, err := os.Stat(configPath); os.IsNotExist(err) {
		log.Printf("INFO: Config file not found at %s, using defaults", configPath)
		return &TomlConfig{}, nil
	}

	var config TomlConfig
	if _, err := toml.DecodeFile(configPath, &config); err != nil {
		return nil, err
	}

	log.Printf("INFO: Loaded config from %s", configPath)
	return &config, nil
}

func Load() *Config {
	// 加载TOML配置文件
	tomlConfig, err := loadTomlConfig()
	if err != nil {
		log.Printf("ERROR: Failed to load config file: %v", err)
		log.Printf("INFO: Using default configuration")
		tomlConfig = &TomlConfig{}
	}

	// 设置默认值
	config := &Config{
		SocketPath:    DefaultSocketPath,
		CheckInterval: DefaultCheckInterval,
		CacheDir:      getDefaultCacheDir(),
		AiModuleName:  "gemini",
		AiRuleURL:     "",
		AiAPIKey:      "",
	}

	// 从TOML配置中覆盖设置
	if tomlConfig.App.SocketPath != "" {
		config.SocketPath = tomlConfig.App.SocketPath
	}

	if tomlConfig.App.CheckInterval != "" {
		if duration, err := time.ParseDuration(tomlConfig.App.CheckInterval); err == nil {
			config.CheckInterval = duration
		} else {
			log.Printf("WARN: Invalid check_interval format '%s', using default", tomlConfig.App.CheckInterval)
		}
	}

	if tomlConfig.App.CacheDir != "" {
		config.CacheDir = tomlConfig.App.CacheDir
	}

	if tomlConfig.AI.ModuleName != "" {
		config.AiModuleName = tomlConfig.AI.ModuleName
	}

	if tomlConfig.AI.BaseURL != "" {
		config.AiRuleURL = tomlConfig.AI.BaseURL
	}

	if tomlConfig.AI.APIKey != "" {
		config.AiAPIKey = tomlConfig.AI.APIKey
	}

	// 检查必要的配置
	if config.AiAPIKey == "" {
		log.Println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
		log.Println("!!! WARNING: No AI API key configured in config.toml.         !!!")
		log.Printf("!!! Please set ai.api_key in %s", getConfigPath())
		log.Println("!!! The application will not be able to fetch lyrics.         !!!")
		log.Println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
	}

	return config
}
