package app

import (
	"context"
	"fmt"
	"go-backend/internal/config"
	"go-backend/internal/ipc"
	"go-backend/internal/lyrics"
	"go-backend/internal/player"
	"os"
	"sync"
	"time"

	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
)

const timeShit = 0.1 // s

type App struct {
	cfg            *config.Config
	ipcServer      *ipc.Server
	lyricsProvider *lyrics.Provider
	currentSong    string
	mutex          sync.Mutex

	// 歌词调度器控制
	schedulerMutex  sync.Mutex
	schedulerCancel context.CancelFunc
	schedulerActive bool
}

func New(cfg *config.Config) *App {
	// 设置 zerolog 的全局配置
	zerolog.TimeFieldFormat = time.RFC3339
	log.Logger = log.Output(zerolog.ConsoleWriter{Out: os.Stderr})

	// 创建歌词提供商
	lyricsProvider, err := lyrics.NewProvider(cfg.App.CacheDir, cfg.AI.ModuleName, cfg.AI.BaseURL, cfg.AI.APIKey, cfg.Redis)
	if err != nil {
		log.Fatal().Err(err).Msg("Failed to create lyrics provider")
	}

	return &App{
		cfg:            cfg,
		ipcServer:      ipc.NewServer(cfg.App.SocketPath),
		lyricsProvider: lyricsProvider,
	}
}
func (a *App) Run() {
	if err := os.MkdirAll(a.cfg.App.CacheDir, 0755); err != nil {
		log.Fatal().Err(err).Str("cache_dir", a.cfg.App.CacheDir).Msg("Failed to create cache directory")
	}
	log.Info().Str("cache_dir", a.cfg.App.CacheDir).Msg("Lyrics cache directory")

	if err := a.ipcServer.Start(); err != nil {
		log.Fatal().Err(err).Msg("Failed to start IPC server")
	}
	defer a.ipcServer.Close()

	ticker := time.NewTicker(a.cfg.App.CheckInterval)
	defer ticker.Stop()

	log.Info().Msg("Starting player check loop...")
	for {
		a.updateSongInfo()
		<-ticker.C
	}
}

func (a *App) updateSongInfo() {
	songIdentifier, err := player.GetCurrentSong()
	if err != nil {
		a.ipcServer.Broadcast("No music playing...")
		return
	}

	a.mutex.Lock()
	if songIdentifier == a.currentSong {
		a.mutex.Unlock()
		return
	}
	log.Info().Msg("-----------------------------------------------------")
	log.Info().Str("song", songIdentifier).Msg("New song detected")
	a.currentSong = songIdentifier
	a.mutex.Unlock()

	a.ipcServer.Broadcast(fmt.Sprintf("... Searching for lyrics for %s ...", songIdentifier))

	// 创建可取消的上下文，确保如果切换歌曲，可以取消未完成的歌词获取
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel() // 确保资源最终被释放

	lyricsText, err := a.lyricsProvider.GetLyrics(ctx, songIdentifier)
	if err != nil {
		log.Error().Err(err).Msg("Failed to get lyrics")
		a.ipcServer.Broadcast(fmt.Sprintf("Error getting lyrics: %v", err))
		return
	}

	a.startLyricScheduler(lyricsText, player.GetCurrentPlayTime)
}

func getLyricIndexAtTime(lines []lyrics.Line, t float64) int {
	if len(lines) == 0 {
		return -1
	}

	// 如果时间在第一行歌词之前，返回 -1
	if t < lines[0].Time {
		return -1
	}

	// 二分查找提高效率
	left, right := 0, len(lines)-1
	result := -1

	for left <= right {
		mid := (left + right) / 2
		if lines[mid].Time <= t {
			result = mid
			left = mid + 1
		} else {
			right = mid - 1
		}
	}

	return result
}
func (a *App) startLyricScheduler(lrc string, getCurrentTime func() float64) {
	// 使用专门的锁保护调度器状态
	a.schedulerMutex.Lock()
	defer a.schedulerMutex.Unlock()

	// 停止之前的歌词调度器（如果有）
	if a.schedulerCancel != nil {
		log.Info().Msg("Stopping previous lyric scheduler")
		a.schedulerCancel()
		a.schedulerCancel = nil

		// 为了确保旧调度器有时间退出，等待一小段时间
		// 这个短暂等待不会影响用户体验，但确保资源正确释放
		time.Sleep(50 * time.Millisecond)
	}

	lines := lyrics.ParseLRC(lrc)
	if len(lines) == 0 {
		log.Warn().Msg("No lyrics lines found, broadcasting raw text")
		a.ipcServer.Broadcast(lrc)
		return
	}

	log.Info().Int("lines_count", len(lines)).Msg("Starting lyric scheduler")

	// 创建新的上下文和取消函数
	ctx, cancel := context.WithCancel(context.Background())
	a.schedulerCancel = cancel
	a.schedulerActive = true

	go func() {
		// 防止goroutine泄漏
		// 通过闭包我们已经捕获了cancel函数和上下文

		defer func() {
			a.schedulerMutex.Lock()
			a.schedulerActive = false
			// 只有当这是最新的调度器时才清除（通过闭包捕获的方式进行比较）
			select {
			case <-ctx.Done():
				// 如果上下文已被取消，则说明这个调度器被停止了
				log.Debug().Msg("Cleaning up cancelled scheduler")
			default:
				// 如果上下文尚未取消，说明这是最新的调度器被自然结束（如歌曲结束）
				// 这时才清除schedulerCancel引用
				log.Debug().Msg("Cleaning up finished scheduler")
				a.schedulerCancel = nil
			}
			a.schedulerMutex.Unlock()
			log.Info().Msg("Lyric scheduler stopped")
		}()

		var (
			lastIndex = -2 // 确保第一次广播
		)

		// 主循环：每50ms检查一次
		ticker := time.NewTicker(50 * time.Millisecond)
		defer ticker.Stop()

		log.Info().Msg("Lyric scheduler started with real-time sync")

		for {
			select {
			case <-ticker.C:
				// 每次都重新获取播放器时间，避免累积误差
				currentTime := getCurrentTime()

				// 处理异常时间
				if currentTime < 0 {
					log.Warn().Float64("player_time", currentTime).Msg("Invalid player time")
					continue
				}

				// 查找当前时间对应的歌词（提前0.5秒显示）
				lookupTime := currentTime + timeShit // 提前0.5秒查找歌词
				newIndex := getLyricIndexAtTime(lines, lookupTime)

				// 只有索引改变时才处理
				if newIndex != lastIndex {
					if newIndex >= 0 && newIndex < len(lines) {
						lyric := lines[newIndex]
						// 计算时间差时考虑0.5秒的提前量
						timeDiff := (currentTime - lyric.Time + timeShit) * 1000 // 转换为毫秒

						// 调整时间窗口：考虑0.5秒提前，允许-100ms到+100ms的误差
						if timeDiff >= -100 && timeDiff <= 100 { // 允许100ms的前后误差
							log.Info().
								Int("index", newIndex).
								Float64("player_time", currentTime).
								Float64("lyric_time", lyric.Time).
								Float64("time_diff_ms", timeDiff).
								Str("lyric", lyric.Text).
								Msg("Broadcasting lyric")

							a.ipcServer.Broadcast(lyric.Text)
						} else {
							// 如果超出时间窗口，记录警告但仍然广播
							log.Warn().
								Int("index", newIndex).
								Float64("player_time", currentTime).
								Float64("lyric_time", lyric.Time).
								Float64("time_diff_ms", timeDiff).
								Str("lyric", lyric.Text).
								Msg("Lyric timing outside window, but broadcasting anyway")

							a.ipcServer.Broadcast(lyric.Text)
						}
					} else if newIndex == -1 && currentTime+timeShit < lines[0].Time {
						// 在第一句歌词之前
						if lastIndex != -1 {
							a.ipcServer.Broadcast("♪ 即将开始... ♪")
						}
					}

					lastIndex = newIndex
				}

				// 检查歌曲是否结束
				if len(lines) > 0 && currentTime > lines[len(lines)-1].Time+5.0 {
					log.Info().
						Float64("current_time", currentTime).
						Float64("last_lyric_time", lines[len(lines)-1].Time).
						Msg("Song finished")
					a.ipcServer.Broadcast("♪ 歌曲结束 ♪")
					return
				}

			case <-ctx.Done():
				log.Info().Msg("Lyric scheduler cancelled")
				return
			}
		}
	}()
}
