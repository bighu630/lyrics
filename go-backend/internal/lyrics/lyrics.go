package lyrics

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"go-backend/internal/config"
	"go-backend/internal/player"
	"go-backend/pkg/ai"
	"go-backend/pkg/ai/gemini"
	"go-backend/pkg/ai/openai"
	"go-backend/pkg/music"
	musiccache "go-backend/pkg/musicCache"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/rs/zerolog/log"
)

type Line struct {
	Time float64
	Text string
}

type Provider struct {
	cacheDir     string
	aiClient     ai.AiInterface
	musicManager *music.Manager
}

type SongInfo struct {
	Title    string  `json:"title"`
	Artist   string  `json:"artist"`
	Duration float64 `json:"duration"` // 歌曲时长（秒）
	IsSong   bool    `json:"is_song"`
}

func formatQuerySong(title string) string {
	return fmt.Sprintf(`请精确地按照以下JSON格式提取歌曲信息:
	输入是一个媒体标题字符串，如果标题中包含歌曲信息，请返回符合格式的JSON；
	{"is_song": true, "title": "歌曲标题", "artist": "演唱者"}。
	如果你判读这不是一首歌请返回{"is_song": false}。
	eg: "山吹菌 - 【绝美戏腔】少年霜/提糯-非李" 返回{is_song: true, title:"非李", artist"少年霜"}。

	!!!请注意，"title" 和 "artist" 必须准确，否则将被视为错误.
	!!!请注意，如果标题中有繁体字，返回的json中需要转换为简体字.

	输入媒体标题是: %s`, title)
}

func extractSongJSON(input string) string {
	// 1. 拍平（去掉换行和多余空白）
	flat := strings.ReplaceAll(input, "\n", "")
	flat = strings.ReplaceAll(flat, "\r", "")
	flat = strings.ReplaceAll(flat, "\t", "")
	flat = regexp.MustCompile(`\s+`).ReplaceAllString(flat, " ")

	// 2. 匹配包含 is_song 的 JSON
	re := regexp.MustCompile(`\{[^{}]*"is_song"\s*:\s*(true|false)[^{}]*\}`)

	return re.FindString(flat)
}

func NewProvider(cacheDir string, aiCfg config.AIConfig, lrcCfg config.LrcProviderConfig) (*Provider, error) {
	musicManager, err := music.CreateDefaultManager(lrcCfg)
	if err != nil {
		return nil, fmt.Errorf("failed to create music manager: %w", err)
	}
	var aiClient ai.AiInterface

	if aiCfg.ModuleName == "gemini" {
		aiClient = gemini.NewGemini(aiCfg.APIKey, "")
	} else {
		aiClient = openai.NewOpenAi(aiCfg.APIKey, aiCfg.ModuleName, aiCfg.BaseURL)
	}

	return &Provider{
		cacheDir:     cacheDir,
		aiClient:     aiClient,
		musicManager: musicManager,
	}, nil
}

func (p *Provider) GetLyrics(ctx context.Context, songIdentifier string) (string, error) {
	ctx, cancel := context.WithTimeout(ctx, 20*time.Second)
	defer cancel()

	var rawSongInfo string
	var err error
	const maxRetries = 3
	rawSongInfo, err = musiccache.GetCache(songIdentifier)
	if err == nil && rawSongInfo != "" {
		log.Info().
			Str("song_identifier", songIdentifier).
			Msg("Cache HIT for song. Loading lyrics from Redis")
	} else {
		for i := range maxRetries {
			rawSongInfo, err = p.aiClient.HandleText(formatQuerySong(songIdentifier))
			if err == nil {
				rawSongInfo = extractSongJSON(rawSongInfo)
				if len(rawSongInfo) > 0 {
					musiccache.AddCache(songIdentifier, rawSongInfo)
				}
				break
			}
			log.Warn().
				Err(err).
				Int("attempt", i+1).
				Int("max_retries", maxRetries).
				Msg("Failed to query AI service")
			time.Sleep(1 * time.Second) // Wait a bit before retrying
		}
	}
	rawSongInfo = extractSongJSON(rawSongInfo)

	if err != nil {
		return "", fmt.Errorf("failed to query AI service after %d attempts: %w", maxRetries, err)
	}

	var songInfo SongInfo
	if unmarshalErr := json.Unmarshal([]byte(rawSongInfo), &songInfo); unmarshalErr != nil {
		return "", fmt.Errorf("failed to parse AI response: %w", unmarshalErr)
	}

	if !songInfo.IsSong {
		return fmt.Sprintf("'%s' is not a song.", songIdentifier), nil
	}
	log.Info().
		Str("song_title", songInfo.Title).
		Str("song_identifier", songIdentifier).
		Msg("AI returned song keywords")

	cacheFilename := sanitizeFilename(songInfo.Title+"-"+songInfo.Artist) + ".lrc"
	cacheFilepath := filepath.Join(p.cacheDir, cacheFilename)

	if cachedLyrics, readErr := os.ReadFile(cacheFilepath); readErr == nil {
		log.Info().
			Str("cache_file", cacheFilepath).
			Msg("Cache HIT. Loading lyrics from file")
		return string(cachedLyrics), nil
	}
	log.Info().
		Str("song_identifier", songIdentifier).
		Msg("Cache MISS. Fetching from API")

	// 获取歌曲时长
	songInfo.Duration = player.GetCurrentDuration()
	log.Info().
		Float64("duration", songInfo.Duration).
		Str("title", songInfo.Title).
		Str("artist", songInfo.Artist).
		Msg("Got song duration")

	// 使用音乐管理器直接获取歌词（封装了搜索+获取歌词的过程）
	lyrics, err := p.musicManager.GetLyricsByInfo(ctx, songInfo.Title, songInfo.Artist, songInfo.Duration)
	if err != nil {
		return "", fmt.Errorf("failed to get lyrics for '%s - %s': %w", songInfo.Title, songInfo.Artist, err)
	}

	log.Info().
		Str("cache_file", cacheFilepath).
		Msg("Saving new lyrics to cache file")
	if err := os.WriteFile(cacheFilepath, []byte(lyrics), 0644); err != nil {
		log.Error().
			Err(err).
			Str("cache_file", cacheFilepath).
			Msg("Failed to write to cache file")
	}

	return lyrics, nil
}

func ParseLRC(lrc string) []Line {
	re := regexp.MustCompile(`\[(\d{2}):(\d{2})(?:\.(\d{1,3}))?\](.*)`)
	scanner := bufio.NewScanner(strings.NewReader(lrc))
	var result []Line

	for scanner.Scan() {
		line := scanner.Text()
		matches := re.FindAllStringSubmatch(line, -1)
		for _, match := range matches {
			min, _ := strconv.Atoi(match[1])
			sec, _ := strconv.Atoi(match[2])
			ms := 0
			if match[3] != "" {
				msStr := match[3]
				ms, _ = strconv.Atoi(msStr)
				// 根据毫秒字符串的长度来正确处理毫秒值
				switch len(msStr) {
				case 1:
					ms *= 100 // 1位数时，如 .1 表示 100ms
				case 2:
					ms *= 10 // 2位数时，如 .49 表示 490ms
				case 3:
					// 3位数时，如 .490 表示 490ms，不需要乘
				}
			}
			text := strings.TrimSpace(match[4])
			timestamp := float64(min*60+sec) + float64(ms)/1000
			result = append(result, Line{Time: timestamp, Text: text})
		}
	}
	sort.Slice(result, func(i, j int) bool { return result[i].Time < result[j].Time })
	return result
}

func sanitizeFilename(name string) string {
	re := regexp.MustCompile(`[\\/:*?"<>|]`)
	return re.ReplaceAllString(name, "-")
}
