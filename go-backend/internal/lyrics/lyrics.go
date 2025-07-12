package lyrics

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"go-backend/internal/player"
	"go-backend/pkg/ai"
	"go-backend/pkg/ai/gemini"
	"go-backend/pkg/ai/openai"
	"go-backend/pkg/music"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"
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
	return fmt.Sprintf(`请精确地按照以下JSON格式提取歌曲信息: {"is_song": true, "title": "歌曲标题", "artist": "演唱者"}。  输入是一个媒体标题，如果标题中包含歌曲信息，请返回符合格式的JSON；否则，返回{"is_song": false}。 请注意，"title" 和 "artist" 必须准确，否则将被视为错误，切记不要任何markdown格式，并将繁体中文转换为简体。 媒体标题是：%s`, title)
}

func NewProvider(cacheDir, moduleName, url, apiKey string) (*Provider, error) {
	musicManager, err := music.CreateDefaultManager()
	if err != nil {
		return nil, fmt.Errorf("failed to create music manager: %w", err)
	}
	var aiClient ai.AiInterface

	if moduleName == "gemini" {
		aiClient = gemini.NewGemini(apiKey, "")
	} else {
		aiClient = openai.NewOpenAi(apiKey, moduleName, url)
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
	for i := range maxRetries {
		rawSongInfo, err = p.aiClient.HandleText(formatQuerySong(songIdentifier))
		if err == nil {
			break
		}
		log.Printf("WARN: Failed to query Gemini (attempt %d/%d): %v", i+1, maxRetries, err)
		time.Sleep(1 * time.Second) // Wait a bit before retrying
	}

	if err != nil {
		return "", fmt.Errorf("failed to query Gemini after %d attempts: %w", maxRetries, err)
	}

	var songInfo SongInfo
	if unmarshalErr := json.Unmarshal([]byte(rawSongInfo), &songInfo); unmarshalErr != nil {
		return "", fmt.Errorf("failed to parse Gemini response: %w", unmarshalErr)
	}

	if !songInfo.IsSong {
		return fmt.Sprintf("'%s' is not a song.", songIdentifier), nil
	}
	log.Printf("INFO: Gemini returned keywords: '%s'", songInfo.Title)

	cacheFilename := sanitizeFilename(songInfo.Title+"-"+songInfo.Artist) + ".lrc"
	cacheFilepath := filepath.Join(p.cacheDir, cacheFilename)

	if cachedLyrics, readErr := os.ReadFile(cacheFilepath); readErr == nil {
		log.Printf("INFO: Cache HIT. Loading lyrics from %s", cacheFilepath)
		return string(cachedLyrics), nil
	}
	log.Printf("INFO: Cache MISS for %s. Fetching from API.", songIdentifier)

	// 获取歌曲时长
	songInfo.Duration = player.GetCurrentDuration()
	log.Printf("INFO: Got song duration: %.2f seconds for '%s - %s'", songInfo.Duration, songInfo.Title, songInfo.Artist)

	// 使用音乐管理器直接获取歌词（封装了搜索+获取歌词的过程）
	lyrics, err := p.musicManager.GetLyricsByInfo(ctx, songInfo.Title, songInfo.Artist, songInfo.Duration)
	if err != nil {
		return "", fmt.Errorf("failed to get lyrics for '%s - %s': %w", songInfo.Title, songInfo.Artist, err)
	}

	log.Printf("INFO: Saving new lyrics to cache file: %s", cacheFilepath)
	if err := os.WriteFile(cacheFilepath, []byte(lyrics), 0644); err != nil {
		log.Printf("ERROR: Failed to write to cache file %s: %v", cacheFilepath, err)
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
