package lyrics

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"go-backend/pkg/ai"
	"go-backend/pkg/ai/gemini"
	"log"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"
)

type NeteaseSearchResponse struct {
	Result struct {
		Songs []struct {
			ID      int    `json:"id"`
			Name    string `json:"name"`
			Artists []struct {
				Name string `json:"name"`
			}
		} `json:"songs"`
	} `json:"result"`
}

type NeteaseLyricResponse struct {
	Lrc struct {
		Lyric string `json:"lyric"`
	} `json:"lrc"`
	Tlyric struct {
		Lyric string `json:"lyric"`
	} `json:"tlyric"`
}

type Line struct {
	Time float64
	Text string
}

type Provider struct {
	cacheDir     string
	geminiClient ai.AiInterface
}

type SongInfo struct {
	Title  string `json:"title"`
	Artist string `json:"artist"`
	IsSong bool   `json:"is_song"`
}

func formatQuerySong(title string) string {
	return fmt.Sprintf(`请精确地按照以下JSON格式提取歌曲信息: {"is_song": true, "title": "歌曲标题", "artist": "演唱者"}。  输入是一个媒体标题，如果标题中包含歌曲信息，请返回符合格式的JSON；否则，返回{"is_song": false}。 请注意，"title" 和 "artist" 必须准确，否则将被视为错误，切记不要任何markdown格式，并将繁体中文转换为简体。 媒体标题是：%s`, title)
}

func NewProvider(cacheDir, geminiAPIKey string) *Provider {
	return &Provider{
		cacheDir:     cacheDir,
		geminiClient: gemini.NewGemini(geminiAPIKey, ""),
	}
}

func (p *Provider) GetLyrics(ctx context.Context, songIdentifier string) (string, error) {
	ctx, cancel := context.WithTimeout(ctx, 20*time.Second)
	defer cancel()

	rawSongInfo, err := p.geminiClient.HandleText(formatQuerySong(songIdentifier))
	if err != nil {
		return "", fmt.Errorf("failed to query Gemini: %w", err)
	}
	var songInfo SongInfo
	if err := json.Unmarshal([]byte(rawSongInfo), &songInfo); err != nil {
		return "", fmt.Errorf("failed to parse Gemini response: %w", err)
	}

	if !songInfo.IsSong {
		return fmt.Sprintf("'%s' is not a song.", songIdentifier), nil
	}
	log.Printf("INFO: Gemini returned keywords: '%s'", songInfo.Title)

	cacheFilename := sanitizeFilename(songInfo.Title+"-"+songInfo.Artist) + ".lrc"
	cacheFilepath := filepath.Join(p.cacheDir, cacheFilename)

	if cachedLyrics, err := os.ReadFile(cacheFilepath); err == nil {
		log.Printf("INFO: Cache HIT. Loading lyrics from %s", cacheFilepath)
		return string(cachedLyrics), nil
	}
	log.Printf("INFO: Cache MISS for %s. Fetching from API.", songIdentifier)

	songID, err := p.searchSongID(ctx, songInfo)
	if err != nil {
		return "", fmt.Errorf("failed to find song ID for '%s': %w", songInfo.Title, err)
	}

	lyrics, err := p.fetchNeteaseLyrics(ctx, songID)
	if err != nil {
		return "", fmt.Errorf("failed to fetch lyrics for ID %d: %w", songID, err)
	}

	log.Printf("INFO: Saving new lyrics to cache file: %s", cacheFilepath)
	if err := os.WriteFile(cacheFilepath, []byte(lyrics), 0644); err != nil {
		log.Printf("ERROR: Failed to write to cache file %s: %v", cacheFilepath, err)
	}

	return lyrics, nil
}

func (p *Provider) searchSongID(ctx context.Context, si SongInfo) (int, error) {
	searchURL := fmt.Sprintf("https://music.163.com/api/search/get/web?csrf_token=hlpretag&hlposttag=&s=%s&type=1", url.QueryEscape(si.Title+" "+si.Artist))
	log.Printf("INFO: Searching for song ID with URL: %s", searchURL)

	req, err := http.NewRequestWithContext(ctx, "GET", searchURL, nil)
	if err != nil {
		return 0, fmt.Errorf("failed to create search request: %w", err)
	}

	// 从环境变量获取网易云音乐 Cookie
	cookie := os.Getenv("NETEASE_COOKIE")
	if cookie != "" {
		req.Header.Set("Cookie", cookie)
	}

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return 0, fmt.Errorf("failed to send search request: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return 0, fmt.Errorf("search API request failed with status %d", resp.StatusCode)
	}

	var searchResp NeteaseSearchResponse
	if err := json.NewDecoder(resp.Body).Decode(&searchResp); err != nil {
		return 0, fmt.Errorf("failed to decode search response")
	}

	if len(searchResp.Result.Songs) == 0 {
		return 0, fmt.Errorf("no songs found for '%s'", si.Title)
	}
	return findByContain(searchResp, si.Artist, si.Title), nil
}

func (p *Provider) fetchNeteaseLyrics(ctx context.Context, songID int) (string, error) {
	lyricURL := fmt.Sprintf("http://music.163.com/api/song/lyric?os=pc&id=%d&lv=-1&kv=-1&tv=-1", songID)
	log.Printf("INFO: Fetching lyrics with URL: %s", lyricURL)
	req, err := http.NewRequestWithContext(ctx, "GET", lyricURL, nil)
	if err != nil {
		return "", fmt.Errorf("failed to create lyric request: %w", err)
	}

	// 从环境变量获取网易云音乐 Cookie
	cookie := os.Getenv("NETEASE_COOKIE")
	if cookie != "" {
		req.Header.Set("Cookie", cookie)
	}

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("failed to send lyric request: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("lyric API request failed with status %d", resp.StatusCode)
	}

	var lyricResp NeteaseLyricResponse
	if err := json.NewDecoder(resp.Body).Decode(&lyricResp); err != nil {
		return "", fmt.Errorf("failed to decode lyric response: %w", err)
	}

	originalLyrics := lyricResp.Lrc.Lyric
	translatedLyrics := lyricResp.Tlyric.Lyric

	if originalLyrics == "" {
		log.Printf("INFO: No lyrics found for song ID %d. It might be instrumental.", songID)
		return "(Instrumental or no lyrics found)", nil
	}

	originalLines := parseLyrics(originalLyrics)
	translatedLines := parseLyrics(translatedLyrics)

	var timestamps []string
	for t := range originalLines {
		timestamps = append(timestamps, t)
	}
	sort.Strings(timestamps)

	var combinedLyrics strings.Builder
	for _, t := range timestamps {
		combinedLyrics.WriteString(fmt.Sprintf("[%s]%s\n", t, originalLines[t]))
		if translated, ok := translatedLines[t]; ok {
			combinedLyrics.WriteString(fmt.Sprintf("[%s]%s\n", t, translated))
		}
	}

	return strings.TrimSpace(combinedLyrics.String()), nil
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

func parseLyrics(lyricText string) map[string]string {
	lines := make(map[string]string)
	re := regexp.MustCompile(`\[(\d{2}:\d{2}\.\d{2,3})\](.*)`)
	matches := re.FindAllStringSubmatch(lyricText, -1)
	for _, match := range matches {
		if len(match) > 2 {
			time := match[1]
			text := strings.TrimSpace(match[2])
			if text != "" {
				lines[time] = text
			}
		}
	}
	return lines
}

func normalizeString(s string) string {
	// 转换为小写并去除所有空格
	return strings.ReplaceAll(strings.ToLower(s), " ", "")
}

func containsIgnoreCase(s1, s2 string) bool {
	norm1, norm2 := normalizeString(s1), normalizeString(s2)
	return strings.Contains(norm1, norm2) || strings.Contains(norm2, norm1)
}

func findByContain(resp NeteaseSearchResponse, targetArtist, targetTitle string) int {
	for _, song := range resp.Result.Songs {
		// 判断歌曲名包含关系
		if !containsIgnoreCase(song.Name, targetTitle) {
			continue
		}

		// 判断歌手名包含关系，artists 可能有多个，只要一个满足就算
		matchedArtist := false
		for _, artist := range song.Artists {
			if containsIgnoreCase(artist.Name, targetArtist) {
				matchedArtist = true
				break
			}
		}
		if matchedArtist {
			return song.ID
		}
	}
	return 0
}
