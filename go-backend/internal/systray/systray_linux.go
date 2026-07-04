//go:build linux

// Package systray implements a pure-Go StatusNotifierItem (freedesktop
// system tray protocol) using the D-Bus session bus.  It has no CGO
// dependency and does not link GTK, so it is safe to use alongside
// gotk4/GTK4 in the same process.
package systray

import (
	"fmt"
	"os"
	"sync"

	"github.com/godbus/dbus/v5"
)

// MenuItem describes one item in the tray's right-click menu.
type MenuItem struct {
	ID      int32
	Label   string
	Enabled bool
	Clicked func() // called when the menu item is activated
}

// Run registers a StatusNotifierItem on the session bus and blocks
// until the process exits.  The tray icon will be shown in any
// compliant system tray (Waybar tray, KDE, GNOME extension, etc.).
func Run(items []MenuItem, iconBytes []byte) error {
	conn, err := dbus.ConnectSessionBus()
	if err != nil {
		return fmt.Errorf("dbus connect: %w", err)
	}
	defer conn.Close()

	itemPath := dbus.ObjectPath("/StatusNotifierItem")
	menuPath := dbus.ObjectPath("/com/canonical/dbusmenu")

	// Assign stable IDs to menu items
	for i := range items {
		items[i].ID = int32(100 + i)
	}

	// ── StatusNotifierItem ──────────────────────────────────────
	sni := &sniObj{iconData: iconBytes}
	if err := conn.Export(sni, itemPath, "org.kde.StatusNotifierItem"); err != nil {
		return fmt.Errorf("export sni: %w", err)
	}

	// ── DBusMenu ────────────────────────────────────────────────
	dm := &dbusMenuObj{items: items}
	if err := conn.Export(dm, menuPath, "com.canonical.dbusmenu"); err != nil {
		return fmt.Errorf("export menu: %w", err)
	}

	// ── Register by owning the service name ─────────────────────
	wellKnownName := fmt.Sprintf("org.kde.StatusNotifierItem-%d-1", os.Getpid())
	if _, err := conn.RequestName(wellKnownName, dbus.NameFlagDoNotQueue); err != nil {
		return fmt.Errorf("request name: %w", err)
	}

	// ── Wait forever (process exit kills this) ──────────────────
	select {}
}

// ── StatusNotifierItem implementation ────────────────────────────

type sniObj struct {
	iconData []byte
}

func (s *sniObj) Identify() (string, *dbus.Error) {
	return "lyrics", nil
}

// ── DBusMenu implementation ──────────────────────────────────────

type dbusMenuObj struct {
	mu       sync.Mutex
	items    []MenuItem
	revision uint32
}

// GetLayout returns the menu layout tree.
// The return type is (revision uint32, layout []interface{}, err *dbus.Error)
// where layout is a nested structure:
//
//	[]interface{}{parentID, props, children}
//	props = map[string]dbus.Variant
//	children = []interface{} (recursive)
func (m *dbusMenuObj) GetLayout(parentID int32, recursionDepth int32, propertyNames []string) (uint32, []interface{}, *dbus.Error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	rev := m.revision
	children := make([]interface{}, 0, len(m.items))
	for _, item := range m.items {
		props := map[string]dbus.Variant{
			"label":   dbus.MakeVariant(item.Label),
			"enabled": dbus.MakeVariant(item.Enabled),
			"type":    dbus.MakeVariant("standard"),
		}
		entry := []interface{}{item.ID, props, []interface{}{}}
		children = append(children, entry)
	}

	rootProps := map[string]dbus.Variant{
		"label":   dbus.MakeVariant("Lyrics"),
		"enabled": dbus.MakeVariant(true),
	}
	root := []interface{}{int32(0), rootProps, children}
	return rev, root, nil
}

// GetGroupProperties returns properties for specific menu items.
func (m *dbusMenuObj) GetGroupProperties(ids []int32, propertyNames []string) ([]interface{}, *dbus.Error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	result := make([]interface{}, 0, len(ids))
	for _, id := range ids {
		for _, item := range m.items {
			if item.ID == id {
				props := map[string]dbus.Variant{
					"label":   dbus.MakeVariant(item.Label),
					"enabled": dbus.MakeVariant(item.Enabled),
				}
				result = append(result, []interface{}{id, props})
				break
			}
		}
	}
	return result, nil
}

// Event handles a menu event (click).
func (m *dbusMenuObj) Event(id int32, eventID string, data dbus.Variant, timestamp uint32) *dbus.Error {
	if eventID != "clicked" {
		return nil
	}

	m.mu.Lock()
	var handler func()
	for _, item := range m.items {
		if item.ID == id {
			handler = item.Clicked
			break
		}
	}
	m.mu.Unlock()

	if handler != nil {
		go handler()
	}
	return nil
}

// AboutToShow is required by the interface but unused.
func (m *dbusMenuObj) AboutToShow(id int32) (bool, *dbus.Error) {
	return false, nil
}

// AboutToShowGroup is required by the interface but unused.
func (m *dbusMenuObj) AboutToShowGroup(ids []int32) ([]int32, []int32, *dbus.Error) {
	return nil, nil, nil
}
