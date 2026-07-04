//go:build windows

package systray

import "fmt"

type MenuItem struct {
	ID      int32
	Label   string
	Enabled bool
	Clicked func()
}

func Run(items []MenuItem, iconBytes []byte) error {
	// No system tray on Windows yet
	return nil
}
