package player

import (
	"errors"
	"fmt"
	"os/exec"
	"regexp"
	"strconv"
	"strings"

	"github.com/rs/zerolog/log"
)

// MPCStatus MPC播放状态
type MPCStatus string

const (
	MPCStatusPlaying MPCStatus = "playing"
	MPCStatusPaused  MPCStatus = "paused"
	MPCStatusStopped MPCStatus = "stopped"
)

// getMPCStatus 获取MPC的播放状态
func getMPCStatus() MPCStatus {
	cmd := exec.Command("mpc", "status")
	output, err := cmd.Output()
	if err != nil {
		log.Debug().Err(err).Msg("MPC not available or error")
		return MPCStatusStopped
	}

	statusText := string(output)
	if strings.Contains(statusText, "[playing]") {
		return MPCStatusPlaying
	} else if strings.Contains(statusText, "[paused]") {
		return MPCStatusPaused
	}
	return MPCStatusStopped
}

// getCurrentSongFromMPC 从MPC获取当前歌曲信息
func getCurrentSongFromMPC() (string, error) {
	cmd := exec.Command("mpc", "current", "-f", "%artist% - %title%")
	output, err := cmd.Output()
	if err != nil {
		return "", err
	}

	song := strings.TrimSpace(string(output))
	if song == "" {
		return "", nil
	}

	log.Debug().Str("song", song).Msg("MPC current song")
	return song, nil
}

// getCurrentPlayTimeFromMPC 从MPC获取当前播放时间
func getCurrentPlayTimeFromMPC() (float64, error) {
	cmd := exec.Command("mpc", "status", "-f", "")
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		// 查找包含时间信息的行，格式通常是 "[playing] #1/1   0:15/3:42 (7%)"
		if strings.Contains(line, "/") && (strings.Contains(line, "[playing]") || strings.Contains(line, "[paused]")) {
			// 使用正则表达式提取时间，格式如 "3:21/4:34"
			re := regexp.MustCompile(`(\d+:\d+)/(\d+:\d+)`)
			matches := re.FindStringSubmatch(line)
			if len(matches) >= 2 {
				currentTime := matches[1]
				seconds := parseTimeToSeconds(currentTime)
				log.Debug().Str("time", currentTime).Float64("seconds", seconds).Msg("MPC current time")
				return seconds, nil
			}
		}
	}

	return 0, nil
}

// parseTimeToSeconds 将时间格式（如 "1:23"）转换为秒数
func parseTimeToSeconds(timeStr string) float64 {
	parts := strings.Split(timeStr, ":")
	if len(parts) == 2 {
		minutes, err1 := strconv.Atoi(parts[0])
		seconds, err2 := strconv.Atoi(parts[1])
		if err1 == nil && err2 == nil {
			return float64(minutes*60 + seconds)
		}
	} else if len(parts) == 3 {
		// 处理 "1:23:45" 格式（小时:分钟:秒）
		hours, err1 := strconv.Atoi(parts[0])
		minutes, err2 := strconv.Atoi(parts[1])
		seconds, err3 := strconv.Atoi(parts[2])
		if err1 == nil && err2 == nil && err3 == nil {
			return float64(hours*3600 + minutes*60 + seconds)
		}
	}
	return 0
}

// getCurrentSongFromPlayerctl 从playerctl获取当前歌曲信息（原有函数）
func getCurrentSongFromPlayerctl() (string, error) {
	cmd := exec.Command("playerctl", "metadata", "--format", `{{artist}} - {{title}}`)
	output, err := cmd.Output()
	if err != nil {
		return "", err
	}
	song := strings.TrimSpace(string(output))
	if len(song) > 2 {
		cmd = exec.Command("playerctl", "status")
		output, err := cmd.Output()
		if err != nil {
			return "", err
		}
		status := strings.TrimSpace(string(output))
		if status == "Stopped" {
			return "", errors.New("no song playing")
		}
	}
	log.Debug().Str("song", song).Msg("Playerctl current song")
	return song, nil
}

// getCurrentPlayTimeFromPlayerctl 从playerctl获取当前播放时间（原有函数）
func getCurrentPlayTimeFromPlayerctl() (float64, error) {
	out, err := exec.Command("playerctl", "position").Output()
	if err != nil {
		return 0, err
	}
	s := strings.TrimSpace(string(out))
	seconds, err := strconv.ParseFloat(s, 64)
	if err != nil {
		return 0, err
	}
	log.Debug().Float64("seconds", seconds).Msg("Playerctl current time")
	return seconds, nil
}

// getCurrentDurationFromMPC 从MPC获取歌曲总时长
func getCurrentDurationFromMPC() (float64, error) {
	cmd := exec.Command("mpc", "status", "-f", "")
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		// 查找包含时间信息的行，格式通常是 "[playing] #1/1   0:15/3:42 (7%)"
		if strings.Contains(line, "/") && (strings.Contains(line, "[playing]") || strings.Contains(line, "[paused]")) {
			// 使用正则表达式提取时间，格式如 "3:21/4:34"
			re := regexp.MustCompile(`(\d+:\d+)/(\d+:\d+)`)
			matches := re.FindStringSubmatch(line)
			if len(matches) >= 3 {
				totalTime := matches[2]
				seconds := parseTimeToSeconds(totalTime)
				log.Debug().Str("duration", totalTime).Float64("seconds", seconds).Msg("MPC total duration")
				return seconds, nil
			}
		}
	}

	return 0, nil
}

// getCurrentDurationFromPlayerctl 从playerctl获取歌曲总时长
func getCurrentDurationFromPlayerctl() (float64, error) {
	out, err := exec.Command("playerctl", "metadata", "mpris:length").Output()
	if err != nil {
		return 0, err
	}

	// playerctl返回的是微秒，需要转换为秒
	microseconds := strings.TrimSpace(string(out))
	if microseconds == "" {
		return 0, fmt.Errorf("empty duration from playerctl")
	}

	us, err := strconv.ParseInt(microseconds, 10, 64)
	if err != nil {
		return 0, err
	}

	seconds := float64(us) / 1000000.0
	log.Debug().Float64("seconds", seconds).Msg("Playerctl total duration")
	return seconds, nil
}

// GetCurrentSong 获取当前歌曲，优先使用MPC，如果MPC暂停则使用playerctl
func GetCurrentSong() (string, error) {
	// 首先尝试MPC
	mpcStatus := getMPCStatus()
	log.Debug().Str("status", string(mpcStatus)).Msg("MPC status")

	if mpcStatus == MPCStatusPlaying {
		song, err := getCurrentSongFromMPC()
		if err == nil && song != "" {
			log.Info().Str("song", song).Msg("Using MPC for current song")
			return song, nil
		}
		log.Warn().Err(err).Msg("MPC is playing but failed to get song info")
	}

	// MPC不可用、暂停或出错时，使用playerctl
	log.Info().Msg("Falling back to playerctl for current song")
	return getCurrentSongFromPlayerctl()
}

// GetCurrentPlayTime 获取当前播放时间，优先使用MPC，如果MPC暂停则使用playerctl
func GetCurrentPlayTime() float64 {
	// 首先尝试MPC
	mpcStatus := getMPCStatus()

	if mpcStatus == MPCStatusPlaying {
		time, err := getCurrentPlayTimeFromMPC()
		if err == nil {
			log.Debug().Float64("time", time).Msg("Using MPC for current time")
			return time
		}
		log.Warn().Err(err).Msg("MPC is playing but failed to get time")
	}

	// MPC不可用、暂停或出错时，使用playerctl
	time, err := getCurrentPlayTimeFromPlayerctl()
	if err != nil {
		log.Warn().Err(err).Msg("Playerctl failed to get time")
		return 0
	}

	log.Debug().Float64("time", time).Msg("Using playerctl for current time")
	return time
}

// GetCurrentDuration 获取当前歌曲总时长，优先使用MPC，如果MPC不可用则使用playerctl
func GetCurrentDuration() float64 {
	// 首先尝试MPC
	mpcStatus := getMPCStatus()

	if mpcStatus == MPCStatusPlaying || mpcStatus == MPCStatusPaused {
		duration, err := getCurrentDurationFromMPC()
		if err == nil && duration > 0 {
			log.Debug().Float64("duration", duration).Msg("Using MPC for duration")
			return duration
		}
		log.Warn().Err(err).Msg("MPC failed to get duration")
	}

	// MPC不可用或出错时，使用playerctl
	duration, err := getCurrentDurationFromPlayerctl()
	if err != nil {
		log.Warn().Err(err).Msg("Playerctl failed to get duration")
		return 0
	}

	log.Debug().Float64("duration", duration).Msg("Using playerctl for duration")
	return duration
}

// IsPlaying 获取当前播放状态，返回是否正在播放
func IsPlaying() bool {
	// 首先尝试MPC
	mpcStatus := getMPCStatus()
	log.Debug().Str("status", string(mpcStatus)).Msg("MPC status check")

	if mpcStatus == MPCStatusPlaying {
		log.Debug().Msg("Using MPC - currently playing")
		return true
	}

	// MPC不可用或停止时，检查playerctl
	log.Debug().Msg("MPC not available/stopped, checking playerctl")

	cmd := exec.Command("playerctl", "status")
	output, err := cmd.Output()
	if err != nil {
		log.Warn().Err(err).Msg("Playerctl status check failed")
		return false
	}

	status := strings.TrimSpace(string(output))
	isPlaying := strings.ToLower(status) == "playing"

	log.Debug().Str("status", status).Bool("playing", isPlaying).Msg("Playerctl status")
	return isPlaying
}
