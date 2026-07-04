#pragma once
#include <string>
#include <functional>

namespace lyrics {

/// Callback type for font size changes.
using FontSizeCallback = std::function<void(int)>;

/// GUI mode interface
void run_gui(const std::string& title, 
             std::function<void(std::function<void(const std::string&)>)> start_lyrics_listener);

/// TUI mode interface
void run_tui(std::function<void(std::function<void(const std::string&)>)> start_lyrics_listener);

} // namespace lyrics
