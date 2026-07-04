#pragma once

#include <string>

namespace lyrics {

/// Player detection interface.
/// Uses MPC first (preferred), falls back to playerctl.
namespace player {

/// Get current song identifier (e.g., "Artist - Title").
std::string get_current_song();

/// Get current playback position in seconds.
double get_current_play_time();

/// Get total duration of current song in seconds.
double get_current_duration();

/// Check if a player is currently playing.
bool is_playing();

} // namespace player
} // namespace lyrics
