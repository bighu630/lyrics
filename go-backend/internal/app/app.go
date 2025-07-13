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
		defer func() {
			a.schedulerMutex.Lock()
			a.schedulerActive = false
			select {
			case <-ctx.Done():
				log.Debug().Msg("Cleaning up cancelled scheduler")
			default:
				log.Debug().Msg("Cleaning up finished scheduler")
				a.schedulerCancel = nil
			}
			a.schedulerMutex.Unlock()
			log.Info().Msg("Lyric scheduler stopped")
		}()

		var (
			lastIndex = -2               // 确保第一次广播
			baseTime  = getCurrentTime() // 获取基准时间
			startTime = time.Now()       // 记录开始时间
		)

		log.Info().
			Float64("base_time", baseTime).
			Msg("Lyric scheduler started with optimized timing")

		for {
			select {
			case <-ctx.Done():
				log.Info().Msg("Lyric scheduler cancelled")
				return
			default:
			}

			// 计算当前播放时间（基于基准时间和经过的时间）
			elapsed := time.Since(startTime).Seconds()
			currentTime := baseTime + elapsed

			// 查找当前时间对应的歌词（提前timeShit秒显示）
			lookupTime := currentTime + timeShit
			newIndex := getLyricIndexAtTime(lines, lookupTime)

			// 只有索引改变时才处理
			if newIndex != lastIndex {
				if newIndex >= 0 && newIndex < len(lines) {
					lyric := lines[newIndex]
					timeDiff := (currentTime - lyric.Time + timeShit) * 1000

					log.Info().
						Int("index", newIndex).
						Float64("current_time", currentTime).
						Float64("lyric_time", lyric.Time).
						Float64("time_diff_ms", timeDiff).
						Str("lyric", lyric.Text).
						Msg("Broadcasting lyric")

					a.ipcServer.Broadcast(lyric.Text)
				} else if newIndex == -1 && currentTime+timeShit < lines[0].Time {
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

			// 计算到下一句歌词的时间差，智能休眠
			var sleepDuration time.Duration

			// 找到下一句歌词
			nextIndex := newIndex + 1
			if nextIndex < len(lines) {
				// 计算到下一句歌词的时间
				nextLyricTime := lines[nextIndex].Time - timeShit // 减去提前量
				timeToNext := nextLyricTime - currentTime

				if timeToNext > 0.5 {
					// 如果距离下一句歌词超过0.5秒，休眠大部分时间，但保留100ms用于精确控制
					sleepDuration = time.Duration((timeToNext-0.1)*1000) * time.Millisecond
				} else if timeToNext > 0.1 {
					// 如果距离较近，休眠较短时间
					sleepDuration = time.Duration((timeToNext*0.5)*1000) * time.Millisecond
				} else {
					// 如果非常接近或已过时，短暂休眠
					sleepDuration = 20 * time.Millisecond
				}
			} else {
				// 没有更多歌词，使用默认间隔
				sleepDuration = 100 * time.Millisecond
			}

			// 限制休眠时间范围
			if sleepDuration < 20*time.Millisecond {
				sleepDuration = 20 * time.Millisecond
			} else if sleepDuration > 1*time.Second {
				sleepDuration = 1 * time.Second
			}

			// 每隔5秒重新同步一次播放器时间，避免累积误差
			if elapsed > 5.0 && int(elapsed)%5 == 0 {
				actualTime := getCurrentTime()
				if actualTime > 0 {
					drift := actualTime - currentTime
					if abs(drift) > 0.5 { // 如果时间偏差超过0.5秒，重新同步
						log.Info().
							Float64("expected_time", currentTime).
							Float64("actual_time", actualTime).
							Float64("drift", drift).
							Msg("Resyncing with player time")

						baseTime = actualTime
						startTime = time.Now()
					}
				}
			}

			// 休眠到下一次检查
			select {
			case <-ctx.Done():
				return
			case <-time.After(sleepDuration):
				// 继续下一次循环
			}
		}
	}()
}

// abs 计算浮点数的绝对值
func abs(x float64) float64 {
	if x < 0 {
		return -x
	}
	return x
}
