//go:build unix

package tui

import (
	"os"
	"syscall"
	"unsafe"
)

const TIOCGWINSZ = 0x5413

type winsize struct {
	Row  uint16
	Col  uint16
	Xpix uint16
	Ypix uint16
}

func getTerminalWidth() int {
	ws := &winsize{}
	ret, _, err := syscall.Syscall(
		syscall.SYS_IOCTL,
		uintptr(os.Stdout.Fd()),
		uintptr(TIOCGWINSZ),
		uintptr(unsafe.Pointer(ws)),
	)
	if ret == 0 && err == 0 && ws.Col > 0 {
		return int(ws.Col)
	}
	return 80 // fallback
}
