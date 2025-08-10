package config

import (
	"os"
	"path/filepath"
	"time"

	"github.com/BurntSushi/toml"
	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
)

const (
	DefaultSocketPath    = "/tmp/lyrics_app.sock"
	DefaultCheckInterval = 5 * time.Second
	DefaultLogLevel      = zerolog.InfoLevel
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

// AppConfig 应用配置 - 同时支持TOML解析和运行时使用
type AppConfig struct {
	SocketPath    string `toml:"socket_path"`
	CheckInterval string `toml:"check_interval"` // TOML中为字符串，解析后转为Duration
	CacheDir      string `toml:"cache_dir"`
	LogLevel      string `toml:"log_level"` // TOML中为字符串，解析后转为zerolog.Level

	// 运行时解析后的字段（不在TOML中序列化）
	ParsedCheckInterval time.Duration `toml:"-"`
	ParsedLogLevel      zerolog.Level `toml:"-"`
}

// AIConfig AI配置
type AIConfig struct {
	ModuleName string `toml:"module_name"`
	APIKey     string `toml:"api_key"`
	BaseURL    string `toml:"base_url"`
}

// LrcProviderConfig 歌词提供商配置
type LrcProviderConfig struct {
	NeteaseCookie string `toml:"netease_cookie"`
}

// Config 主配置结构 - 直接用于TOML解析
type Config struct {
	App AppConfig         `toml:"app"`
	AI  AIConfig          `toml:"ai"`
	Lrc LrcProviderConfig `toml:"lrc"`
}

var logger = log.With().Str("component", "config").Logger()

// parseLogLevel 解析日志级别字符串为zerolog.Level
func parseLogLevel(level string) zerolog.Level {
	switch level {
	case "debug":
		return zerolog.DebugLevel
	case "info":
		return zerolog.InfoLevel
	case "warn", "warning":
		return zerolog.WarnLevel
	case "error":
		return zerolog.ErrorLevel
	case "fatal":
		return zerolog.FatalLevel
	case "panic":
		return zerolog.PanicLevel
	case "disabled":
		return zerolog.Disabled
	default:
		logger.Warn().
			Str("invalid_level", level).
			Str("default_level", DefaultLogLevel.String()).
			Msg("Invalid log level, using default")
		return DefaultLogLevel
	}
}

// PostProcess 处理配置，将字符串转换为相应的类型
func (c *Config) PostProcess() error {
	// 解析检查间隔
	if c.App.CheckInterval != "" {
		if duration, err := time.ParseDuration(c.App.CheckInterval); err == nil {
			c.App.ParsedCheckInterval = duration
		} else {
			logger.Warn().
				Str("check_interval", c.App.CheckInterval).
				Msg("Invalid check_interval format, using default")
			c.App.ParsedCheckInterval = DefaultCheckInterval
		}
	} else {
		c.App.ParsedCheckInterval = DefaultCheckInterval
	}

	// 解析日志级别
	if c.App.LogLevel != "" {
		c.App.ParsedLogLevel = parseLogLevel(c.App.LogLevel)
	} else {
		c.App.ParsedLogLevel = DefaultLogLevel
	}

	return nil
}

// GetCheckInterval 获取解析后的检查间隔
func (c *Config) GetCheckInterval() time.Duration {
	return c.App.ParsedCheckInterval
}

// GetLogLevel 获取解析后的日志级别
func (c *Config) GetLogLevel() zerolog.Level {
	return c.App.ParsedLogLevel
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
		logger.Warn().Err(err).Msg("Cannot get user home directory")
		return "config.toml" // 回退到当前目录
	}

	return filepath.Join(homeDir, ".config", "lyrics", "config.toml")
}

// loadTomlConfig 加载TOML配置文件
func loadTomlConfig() (*Config, error) {
	configPath := getConfigPath()

	// 检查配置文件是否存在
	if _, err := os.Stat(configPath); os.IsNotExist(err) {
		logger.Info().Str("config_path", configPath).Msg("Config file not found, using defaults")
		return &Config{}, nil
	}

	var config Config
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
		tomlConfig = &Config{}
	}

	// 设置默认值
	config := &Config{
		App: AppConfig{
			SocketPath:    DefaultSocketPath,
			CheckInterval: "5s", // 字符串形式的默认值
			CacheDir:      getDefaultCacheDir(),
			LogLevel:      "info", // 字符串形式的默认值
		},
		AI: AIConfig{
			ModuleName: "gemini",
			APIKey:     "",
			BaseURL:    "",
		},
	}

	// 从TOML配置中覆盖App设置
	if tomlConfig.App.SocketPath != "" {
		config.App.SocketPath = tomlConfig.App.SocketPath
	}

	if tomlConfig.App.CheckInterval != "" {
		config.App.CheckInterval = tomlConfig.App.CheckInterval
	}

	if tomlConfig.App.CacheDir != "" {
		config.App.CacheDir = tomlConfig.App.CacheDir
	}

	if tomlConfig.App.LogLevel != "" {
		config.App.LogLevel = tomlConfig.App.LogLevel
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

	config.Lrc = tomlConfig.Lrc

	// 后处理配置（解析字符串为具体类型）
	if err := config.PostProcess(); err != nil {
		logger.Error().Err(err).Msg("Failed to post-process config")
	}

	// 检查必要的配置
	if config.AI.APIKey == "" {
		configPath := getConfigPath()
		logger.Warn().
			Str("config_path", configPath).
			Msg("No AI API key configured - please set ai.api_key in config file. The application will not be able to fetch lyrics.")
	}

	return config
}
