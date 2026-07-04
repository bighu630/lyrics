#pragma once
#include <string>
#include <functional>

namespace lyrics {

/// GUI mode — creates a GTK4 transparent overlay window.
void run_gui(const std::string& title, 
             std::function<void(std::function<void(const std::string&)>)> start_lyrics_listener);

} // namespace lyrics
