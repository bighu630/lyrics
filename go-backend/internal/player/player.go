package player

import (
	"os/exec"
	"strconv"
	"strings"
)

func GetCurrentSong() (string, error) {
	cmd := exec.Command("playerctl", "metadata", "--format", `{{artist}} - {{title}}`)
	output, err := cmd.Output()
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(output)), nil
}

func GetCurrentPlayTime() float64 {
	out, err := exec.Command("playerctl", "position").Output()
	if err != nil {
		return 0
	}
	s := strings.TrimSpace(string(out))
	seconds, err := strconv.ParseFloat(s, 64)
	if err != nil {
		return 0
	}
	return seconds
}
