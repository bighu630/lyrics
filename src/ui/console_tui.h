#pragma once
#include <string>

namespace lyrics {

/// Console TUI display mode.
void run_console_tui(std::function<void(std::function<void(const std::string&)>)> start_lyrics_listener);

} // namespace lyrics
