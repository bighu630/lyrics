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
	"time"
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
	httpClient     *http.Client
	cookie         string
	requestTimeout time.Duration
	maxRetries     int
}

// NewClient 创建新的网易云音乐客户端
func NewClient() *Client {
	return &Client{
		httpClient: &http.Client{
			Timeout: 5 * time.Second, // 设置较短的5秒超时
		},
		cookie:         os.Getenv("NETEASE_COOKIE"),
		requestTimeout: 5 * time.Second,
		maxRetries:     3, // 最多重试3次
	}
}

// GetProviderName 获取提供商名称
func (c *Client) GetProviderName() string {
	return "NetEase Cloud Music"
}

// createTimeoutContext 创建带超时的上下文
func (c *Client) createTimeoutContext(ctx context.Context) (context.Context, context.CancelFunc) {
	// 如果传入的上下文已有超时，使用较短的超时
	deadline, ok := ctx.Deadline()
	if ok {
		timeLeft := time.Until(deadline)
		if timeLeft < c.requestTimeout {
			// 使用现有上下文的较短超时
			return ctx, func() {}
		}
	}
	// 创建新的超时上下文
	return context.WithTimeout(ctx, c.requestTimeout)
}

// SearchSong 搜索歌曲
func (c *Client) SearchSong(ctx context.Context, title, artist string) (string, error) {
	// 创建带超时的上下文
	timeoutCtx, cancel := c.createTimeoutContext(ctx)
	defer cancel()

	searchURL := fmt.Sprintf("https://music.163.com/api/search/get/web?csrf_token=hlpretag&hlposttag=&s=%s&type=1&limit=100", url.QueryEscape(title))
	log.Printf("INFO: [NetEase] Searching for song with URL: %s", searchURL)

	req, err := http.NewRequestWithContext(timeoutCtx, "GET", searchURL, nil)
	if err != nil {
		return "", fmt.Errorf("failed to create search request: %w", err)
	}

	// 设置Cookie
	if c.cookie != "" {
		req.Header.Set("Cookie", c.cookie)
	}

	// 使用带重试的请求
	resp, err := c.doRequestWithRetry(req)
	if err != nil {
		return "", fmt.Errorf("search request failed: %w", err)
	}
	defer resp.Body.Close()

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
	// 创建带超时的上下文
	timeoutCtx, cancel := c.createTimeoutContext(ctx)
	defer cancel()

	lyricURL := fmt.Sprintf("http://music.163.com/api/song/lyric?os=pc&id=%s&lv=-1&kv=-1&tv=-1", songID)
	log.Printf("INFO: [NetEase] Fetching lyrics with URL: %s", lyricURL)

	req, err := http.NewRequestWithContext(timeoutCtx, "GET", lyricURL, nil)
	if err != nil {
		return "", fmt.Errorf("failed to create lyric request: %w", err)
	}

	// 设置Cookie
	if c.cookie != "" {
		req.Header.Set("Cookie", c.cookie)
	}

	// 使用带重试的请求
	resp, err := c.doRequestWithRetry(req)
	if err != nil {
		return "", fmt.Errorf("lyric request failed: %w", err)
	}
	defer resp.Body.Close()

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

// doRequestWithRetry 执行HTTP请求并在失败时重试
func (c *Client) doRequestWithRetry(req *http.Request) (*http.Response, error) {
	var (
		resp    *http.Response
		err     error
		retries = 0
	)

	for retries <= c.maxRetries {
		if retries > 0 {
			log.Printf("INFO: [NetEase] Retrying request to %s (attempt %d/%d)", req.URL, retries, c.maxRetries)
			// 在重试前添加短暂的延迟，避免立即重试
			time.Sleep(time.Duration(retries*500) * time.Millisecond)
		}

		// 创建请求的副本，因为原始请求可能已被消耗
		reqCopy := req.Clone(req.Context())
		resp, err = c.httpClient.Do(reqCopy)

		if err == nil && resp.StatusCode == http.StatusOK {
			return resp, nil // 成功，返回响应
		}

		if err != nil {
			log.Printf("WARN: [NetEase] Request failed: %v (attempt %d/%d)", err, retries+1, c.maxRetries)
		} else {
			log.Printf("WARN: [NetEase] Request returned status %d (attempt %d/%d)", resp.StatusCode, retries+1, c.maxRetries)
			resp.Body.Close()
		}

		retries++
		if retries > c.maxRetries {
			break
		}
	}

	// 所有重试都失败了
	if err != nil {
		return nil, fmt.Errorf("request failed after %d attempts: %w", retries, err)
	}
	return nil, fmt.Errorf("request failed after %d attempts with status %d", retries, resp.StatusCode)
}
