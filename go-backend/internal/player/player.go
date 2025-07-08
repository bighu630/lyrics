package player

import (
	"log"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
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
		log.Printf("DEBUG: MPC not available or error: %v", err)
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

	log.Printf("DEBUG: MPC current song: %s", song)
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
				log.Printf("DEBUG: MPC current time: %s -> %.2f seconds", currentTime, seconds)
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
	log.Printf("DEBUG: Playerctl current song: %s", song)
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
	log.Printf("DEBUG: Playerctl current time: %.2f seconds", seconds)
	return seconds, nil
}

// GetCurrentSong 获取当前歌曲，优先使用MPC，如果MPC暂停则使用playerctl
func GetCurrentSong() (string, error) {
	// 首先尝试MPC
	mpcStatus := getMPCStatus()
	log.Printf("DEBUG: MPC status: %s", mpcStatus)

	if mpcStatus == MPCStatusPlaying {
		song, err := getCurrentSongFromMPC()
		if err == nil && song != "" {
			log.Printf("INFO: Using MPC for current song: %s", song)
			return song, nil
		}
		log.Printf("WARN: MPC is playing but failed to get song info: %v", err)
	}

	// MPC不可用、暂停或出错时，使用playerctl
	log.Printf("INFO: Falling back to playerctl for current song")
	return getCurrentSongFromPlayerctl()
}

// GetCurrentPlayTime 获取当前播放时间，优先使用MPC，如果MPC暂停则使用playerctl
func GetCurrentPlayTime() float64 {
	// 首先尝试MPC
	mpcStatus := getMPCStatus()

	if mpcStatus == MPCStatusPlaying {
		time, err := getCurrentPlayTimeFromMPC()
		if err == nil {
			log.Printf("DEBUG: Using MPC for current time: %.2f", time)
			return time
		}
		log.Printf("WARN: MPC is playing but failed to get time: %v", err)
	}

	// MPC不可用、暂停或出错时，使用playerctl
	time, err := getCurrentPlayTimeFromPlayerctl()
	if err != nil {
		log.Printf("WARN: Playerctl failed to get time: %v", err)
		return 0
	}

	log.Printf("DEBUG: Using playerctl for current time: %.2f", time)
	return time
}
