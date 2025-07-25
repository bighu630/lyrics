package config

import (
	"os"
	"path/filepath"
	"time"

	"github.com/BurntSushi/toml"
	"github.com/rs/zerolog/log"
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

	Redis struct {
		Addr     string `toml:"addr"`
		Password string `toml:"password"`
		DB       int    `toml:"db"`
	} `toml:"redis"`
	Lrc LrcProviderConfig `toml:"lrc"`
}

// AppConfig 应用配置
type AppConfig struct {
	SocketPath    string
	CheckInterval time.Duration
	CacheDir      string
}

// AIConfig AI配置
type AIConfig struct {
	ModuleName string
	APIKey     string
	BaseURL    string
}

// RedisConfig Redis配置
type RedisConfig struct {
	Addr     string
	Password string
	DB       int
}

type LrcProviderConfig struct {
	NeteaseCookie string `json:"netease_cookie"`
}

// Config 主配置结构
type Config struct {
	App   AppConfig
	AI    AIConfig
	Redis RedisConfig
	Lrc   LrcProviderConfig
}

var logger = log.With().Str("component", "config").Logger()

// getConfigPath 获取配置文件路径
func getConfigPath() string {
	// 优先使用 XDG_CONFIG_HOME 环境变量
	if configHome := os.Getenv("XDG_CONFIG_HOME"); configHome != "" {
		return filepath.Join(configHome, "lyrics", "config.toml")
	}

	// 否则使用用户主目录下的 .config
	homeDir, err := os.UserHomeDir()
	if err != nil {
		logger.Warn().Err(err).Msg("Cannot get user home directory")
		return "config.toml" // 回退到当前目录
	}

	return filepath.Join(homeDir, ".config", "lyrics", "config.toml")
}

// loadTomlConfig 加载TOML配置文件
func loadTomlConfig() (*TomlConfig, error) {
	configPath := getConfigPath()

	// 检查配置文件是否存在
	if _, err := os.Stat(configPath); os.IsNotExist(err) {
		logger.Info().Str("config_path", configPath).Msg("Config file not found, using defaults")
		return &TomlConfig{}, nil
	}

	var config TomlConfig
	if _, err := toml.DecodeFile(configPath, &config); err != nil {
		return nil, err
	}

	logger.Info().Str("config_path", configPath).Msg("Loaded config")
	return &config, nil
}

func Load() *Config {
	// 加载TOML配置文件
	tomlConfig, err := loadTomlConfig()
	if err != nil {
		logger.Error().Err(err).Msg("Failed to load config file")
		logger.Info().Msg("Using default configuration")
		tomlConfig = &TomlConfig{}
	}

	// 设置默认值
	config := &Config{
		App: AppConfig{
			SocketPath:    DefaultSocketPath,
			CheckInterval: DefaultCheckInterval,
			CacheDir:      getDefaultCacheDir(),
		},
		AI: AIConfig{
			ModuleName: "gemini",
			APIKey:     "",
			BaseURL:    "",
		},
		Redis: RedisConfig{
			Addr:     "localhost:6379",
			Password: "",
			DB:       0,
		},
	}

	// 从TOML配置中覆盖App设置
	if tomlConfig.App.SocketPath != "" {
		config.App.SocketPath = tomlConfig.App.SocketPath
	}

	if tomlConfig.App.CheckInterval != "" {
		if duration, err := time.ParseDuration(tomlConfig.App.CheckInterval); err == nil {
			config.App.CheckInterval = duration
		} else {
			logger.Warn().
				Str("check_interval", tomlConfig.App.CheckInterval).
				Msg("Invalid check_interval format, using default")
		}
	}

	if tomlConfig.App.CacheDir != "" {
		config.App.CacheDir = tomlConfig.App.CacheDir
	}

	// 从TOML配置中覆盖AI设置
	if tomlConfig.AI.ModuleName != "" {
		config.AI.ModuleName = tomlConfig.AI.ModuleName
	}

	if tomlConfig.AI.BaseURL != "" {
		config.AI.BaseURL = tomlConfig.AI.BaseURL
	}

	if tomlConfig.AI.APIKey != "" {
		config.AI.APIKey = tomlConfig.AI.APIKey
	}

	// 从TOML配置中覆盖Redis设置
	if tomlConfig.Redis.Addr != "" {
		config.Redis.Addr = tomlConfig.Redis.Addr
	}

	if tomlConfig.Redis.Password != "" {
		config.Redis.Password = tomlConfig.Redis.Password
	}

	if tomlConfig.Redis.DB != 0 {
		config.Redis.DB = tomlConfig.Redis.DB
	}
	config.Lrc = tomlConfig.Lrc

	// 检查必要的配置
	if config.AI.APIKey == "" {
		configPath := getConfigPath()
		logger.Warn().
			Str("config_path", configPath).
			Msg("No AI API key configured - please set ai.api_key in config file. The application will not be able to fetch lyrics.")
	}

	return config
}
