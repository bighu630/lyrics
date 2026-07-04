#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace lyrics {

/// System tray interface.
namespace tray {

/// A menu item for the tray's right-click context menu.
struct MenuItem {
    int32_t id;
    std::string label;
    bool enabled;
    std::function<void()> clicked;
};

/// Run the system tray with the given menu items.
/// On Linux, implements the StatusNotifierItem D-Bus protocol.
/// @param items       Menu items for the right-click context menu
/// @param icon_data   PNG icon data (optional, can be empty for no icon)
/// @return false on failure
bool run(std::vector<MenuItem> items, const std::vector<uint8_t>& icon_data);

} // namespace tray
} // namespace lyrics
