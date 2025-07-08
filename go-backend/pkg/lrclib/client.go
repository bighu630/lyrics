package lrclib

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"net/url"
	"strings"
	"time"
)

// Client LRCLib客户端
type Client struct {
	httpClient     *http.Client
	baseURL        string
	requestTimeout time.Duration
	maxRetries     int
}

// LRCLibResponse LRCLib API响应结构
type LRCLibResponse struct {
	ID           int    `json:"id"`
	Name         string `json:"name"`
	TrackName    string `json:"trackName"`
	ArtistName   string `json:"artistName"`
	AlbumName    string `json:"albumName"`
	Duration     int    `json:"duration"`
	Instrumental bool   `json:"instrumental"`
	PlainLyrics  string `json:"plainLyrics"`
	SyncedLyrics string `json:"syncedLyrics"`
}

// LRCLibSearchResponse LRCLib API搜索响应（列表）
type LRCLibSearchResponse []LRCLibResponse

// NewClient 创建新的LRCLib客户端
func NewClient() *Client {
	return &Client{
		httpClient: &http.Client{
			Timeout: 5 * time.Second,
		},
		baseURL:        "https://lrclib.net/api",
		requestTimeout: 5 * time.Second,
		maxRetries:     3,
	}
}

// GetProviderName 返回提供商名称
func (c *Client) GetProviderName() string {
	return "LRCLib"
}

// SearchSong 搜索歌曲（LRCLib直接通过参数搜索，不需要单独的搜索步骤）
func (c *Client) SearchSong(ctx context.Context, title, artist string) (string, error) {
	// LRCLib不需要单独的搜索步骤，直接返回查询参数作为"ID"
	return fmt.Sprintf("%s|%s", title, artist), nil
}

// GetLyrics 获取歌词
func (c *Client) GetLyrics(ctx context.Context, songID string) (string, error) {
	// 解析songID（格式：title|artist）
	parts := strings.Split(songID, "|")
	if len(parts) != 2 {
		return "", fmt.Errorf("invalid song ID format: %s", songID)
	}
	title, artist := parts[0], parts[1]

	return c.getLyricsByInfo(ctx, title, artist, 0)
}

// GetLyricsByInfo 直接通过歌曲信息获取歌词
func (c *Client) GetLyricsByInfo(ctx context.Context, title, artist string, duration float64) (string, error) {
	return c.getLyricsByInfo(ctx, title, artist, int(duration))
}

// getLyricsByInfo 内部方法，通过歌曲信息获取歌词
func (c *Client) getLyricsByInfo(ctx context.Context, title, artist string, duration int) (string, error) {
	timeoutCtx, cancel := context.WithTimeout(ctx, c.requestTimeout)
	defer cancel()

	// 构建查询参数
	params := url.Values{}
	params.Set("track_name", title)
	params.Set("artist_name", artist)
	// 不直接传递 duration 参数，改为在结果中筛选

	searchURL := fmt.Sprintf("%s/search?%s", c.baseURL, params.Encode())

	var resp *http.Response
	var err error

	// 重试机制
	for attempt := 0; attempt <= c.maxRetries; attempt++ {
		if attempt > 0 {
			log.Printf("INFO: [LRCLib] Retrying request (attempt %d/%d)", attempt, c.maxRetries)
			time.Sleep(time.Duration(attempt*500) * time.Millisecond)
		}

		req, reqErr := http.NewRequestWithContext(timeoutCtx, "GET", searchURL, nil)
		if reqErr != nil {
			return "", fmt.Errorf("failed to create request: %w", reqErr)
		}

		// 设置User-Agent
		req.Header.Set("User-Agent", "lyrics-backend/1.0")

		resp, err = c.httpClient.Do(req)
		if err == nil && resp.StatusCode == http.StatusOK {
			break
		}

		if err != nil {
			log.Printf("WARN: [LRCLib] Request failed: %v (attempt %d/%d)", err, attempt+1, c.maxRetries)
		} else {
			log.Printf("WARN: [LRCLib] Request returned status %d (attempt %d/%d)", resp.StatusCode, attempt+1, c.maxRetries)
			resp.Body.Close()
		}

		if attempt == c.maxRetries {
			if err != nil {
				return "", fmt.Errorf("request failed after %d attempts: %w", attempt+1, err)
			}
			return "", fmt.Errorf("request failed after %d attempts with status %d", attempt+1, resp.StatusCode)
		}
	}

	defer resp.Body.Close()

	var lrcResponses LRCLibSearchResponse
	if err := json.NewDecoder(resp.Body).Decode(&lrcResponses); err != nil {
		return "", fmt.Errorf("failed to decode response: %w", err)
	}

	log.Printf("INFO: [LRCLib] Found %d results for '%s - %s'", len(lrcResponses), title, artist)

	if len(lrcResponses) == 0 {
		return "", fmt.Errorf("no lyrics found for '%s - %s'", title, artist)
	}

	// 使用智能匹配算法找到最佳匹配的歌词
	bestMatch := c.findBestMatch(lrcResponses, title, artist, duration)

	// 优先返回同步歌词，如果没有则返回纯文本歌词
	if bestMatch.SyncedLyrics != "" {
		log.Printf("INFO: [LRCLib] Selected synced lyrics for '%s - %s' (duration: %ds, target: %ds)",
			bestMatch.TrackName, bestMatch.ArtistName, bestMatch.Duration, duration)
		return bestMatch.SyncedLyrics, nil
	}

	if bestMatch.PlainLyrics != "" {
		log.Printf("INFO: [LRCLib] Selected plain lyrics for '%s - %s' (duration: %ds, target: %ds)",
			bestMatch.TrackName, bestMatch.ArtistName, bestMatch.Duration, duration)
		return bestMatch.PlainLyrics, nil
	}

	return "", fmt.Errorf("selected result has no lyrics for '%s - %s'", title, artist)
}

// findBestMatch 从搜索结果中找到最佳匹配的歌词
func (c *Client) findBestMatch(responses LRCLibSearchResponse, targetTitle, targetArtist string, targetDuration int) *LRCLibResponse {
	if len(responses) == 0 {
		return nil
	}

	// 先按匹配精确度查找
	var exactMatches []*LRCLibResponse
	var titleMatches []*LRCLibResponse

	for i := range responses {
		response := &responses[i]

		// 完全匹配（标题+艺术家）
		if containsIgnoreCase(response.TrackName, targetTitle) && containsIgnoreCase(response.ArtistName, targetArtist) {
			exactMatches = append(exactMatches, response)
			log.Printf("INFO: [LRCLib] Found exact match: %s by %s (duration: %ds)",
				response.TrackName, response.ArtistName, response.Duration)
		} else if containsIgnoreCase(response.TrackName, targetTitle) {
			// 只匹配标题
			titleMatches = append(titleMatches, response)
			log.Printf("INFO: [LRCLib] Found title match: %s by %s (duration: %ds)",
				response.TrackName, response.ArtistName, response.Duration)
		}
	}

	// 如果有精确匹配，优先从精确匹配中筛选时长
	matchPool := exactMatches
	if len(matchPool) == 0 {
		matchPool = titleMatches
	}
	if len(matchPool) == 0 {
		matchPool = make([]*LRCLibResponse, len(responses))
		for i := range responses {
			matchPool[i] = &responses[i]
		}
	}

	// 如果有时长要求，在匹配结果中筛选最接近的
	if targetDuration > 0 && len(matchPool) > 0 {
		const maxDurationDiff = 3 // 最大允许3秒误差
		bestMatch := matchPool[0]
		minDiff := abs(bestMatch.Duration - targetDuration)

		for _, m := range matchPool {
			diff := abs(m.Duration - targetDuration)
			if diff < minDiff {
				minDiff = diff
				bestMatch = m
			}

			// 如果误差在允许范围内，立即返回
			if diff <= maxDurationDiff {
				log.Printf("INFO: [LRCLib] Found duration match within threshold (%ds diff)", diff)
				return m
			}
		}

		// 返回时长最接近的
		log.Printf("INFO: [LRCLib] Using best duration match with %ds difference", minDiff)
		return bestMatch
	}

	// 如果没有时长要求或没找到合适的，返回第一个匹配结果
	if len(matchPool) > 0 {
		return matchPool[0]
	}

	// 没有任何匹配，返回第一个结果
	return &responses[0]
}

// abs 返回整数的绝对值
func abs(n int) int {
	if n < 0 {
		return -n
	}
	return n
}

// containsIgnoreCase 忽略大小写检查包含关系
func containsIgnoreCase(s, substr string) bool {
	return strings.Contains(strings.ToLower(s), strings.ToLower(substr))
}
