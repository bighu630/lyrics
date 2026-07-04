//go:build windows

package tui

import (
	"golang.org/x/sys/windows"
)

func getTerminalWidth() int {
	handle, err := windows.GetStdHandle(windows.STD_OUTPUT_HANDLE)
	if err != nil || handle == windows.InvalidHandle {
		return 80
	}
	var info windows.ConsoleScreenBufferInfo
	err = windows.GetConsoleScreenBufferInfo(handle, &info)
	if err != nil {
		return 80
	}
	return int(info.Size.X)
}
