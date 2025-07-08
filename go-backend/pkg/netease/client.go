package netease

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"net/url"
	"os"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

// NeteaseSearchResponse 网易云搜索API响应
type NeteaseSearchResponse struct {
	Result struct {
		Songs []struct {
			ID      int    `json:"id"`
			Name    string `json:"name"`
			Artists []struct {
				Name string `json:"name"`
			} `json:"artists"`
		} `json:"songs"`
	} `json:"result"`
}

// NeteaseLyricResponse 网易云歌词API响应
type NeteaseLyricResponse struct {
	Lrc struct {
		Lyric string `json:"lyric"`
	} `json:"lrc"`
	Tlyric struct {
		Lyric string `json:"lyric"`
	} `json:"tlyric"`
}

// Client 网易云音乐客户端
type Client struct {
	httpClient *http.Client
	cookie     string
}

// NewClient 创建新的网易云音乐客户端
func NewClient() *Client {
	return &Client{
		httpClient: &http.Client{},
		cookie:     os.Getenv("NETEASE_COOKIE"),
	}
}

// GetProviderName 获取提供商名称
func (c *Client) GetProviderName() string {
	return "NetEase Cloud Music"
}

// SearchSong 搜索歌曲
func (c *Client) SearchSong(ctx context.Context, title, artist string) (string, error) {
	searchURL := fmt.Sprintf("https://music.163.com/api/search/get/web?csrf_token=hlpretag&hlposttag=&s=%s&type=1&limit=100", url.QueryEscape(title))
	log.Printf("INFO: [NetEase] Searching for song with URL: %s", searchURL)

	req, err := http.NewRequestWithContext(ctx, "GET", searchURL, nil)
	if err != nil {
		return "", fmt.Errorf("failed to create search request: %w", err)
	}

	// 设置Cookie
	if c.cookie != "" {
		req.Header.Set("Cookie", c.cookie)
	}

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return "", fmt.Errorf("failed to send search request: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("search API request failed with status %d", resp.StatusCode)
	}

	var searchResp NeteaseSearchResponse
	if err := json.NewDecoder(resp.Body).Decode(&searchResp); err != nil {
		return "", fmt.Errorf("failed to decode search response: %w", err)
	}

	if len(searchResp.Result.Songs) == 0 {
		return "", fmt.Errorf("no songs found for '%s'", title)
	}

	songID := c.findBestMatch(searchResp, artist, title)
	if songID == 0 {
		return "", fmt.Errorf("no matching song found for '%s' by '%s'", title, artist)
	}

	return strconv.Itoa(songID), nil
}

// GetLyrics 获取歌词
func (c *Client) GetLyrics(ctx context.Context, songID string) (string, error) {
	lyricURL := fmt.Sprintf("http://music.163.com/api/song/lyric?os=pc&id=%s&lv=-1&kv=-1&tv=-1", songID)
	log.Printf("INFO: [NetEase] Fetching lyrics with URL: %s", lyricURL)

	req, err := http.NewRequestWithContext(ctx, "GET", lyricURL, nil)
	if err != nil {
		return "", fmt.Errorf("failed to create lyric request: %w", err)
	}

	// 设置Cookie
	if c.cookie != "" {
		req.Header.Set("Cookie", c.cookie)
	}

	resp, err := c.httpClient.Do(req)
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

	return lyricResp.Lrc.Lyric, nil
}

// findBestMatch 找到最佳匹配的歌曲
func (c *Client) findBestMatch(resp NeteaseSearchResponse, targetArtist, targetTitle string) int {
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
			log.Printf("INFO: [NetEase] Found matching song: %s by %s (ID: %d)", song.Name, song.Artists[0].Name, song.ID)
			return song.ID
		}
	}

	// 如果没有找到完全匹配的，返回第一个匹配标题的
	if len(resp.Result.Songs) > 0 && containsIgnoreCase(resp.Result.Songs[0].Name, targetTitle) {
		log.Printf("INFO: [NetEase] Using first matching song: %s (ID: %d)", resp.Result.Songs[0].Name, resp.Result.Songs[0].ID)
		return resp.Result.Songs[0].ID
	}

	return 0
}

// combineLyrics 合并原文和翻译歌词
func (c *Client) combineLyrics(originalLyrics, translatedLyrics string) string {
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

	return strings.TrimSpace(combinedLyrics.String())
}

// parseLyrics 解析歌词，提取时间戳和歌词内容
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

// normalizeString 标准化字符串（转小写，去空格）
func normalizeString(s string) string {
	return strings.ReplaceAll(strings.ToLower(s), " ", "")
}

// containsIgnoreCase 忽略大小写和空格的包含关系检查
func containsIgnoreCase(s1, s2 string) bool {
	norm1, norm2 := normalizeString(s1), normalizeString(s2)
	return strings.Contains(norm1, norm2) || strings.Contains(norm2, norm1)
}
