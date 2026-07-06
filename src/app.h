#pragma once

#include "config.h"
#include "lyrics/provider.h"
#include "lyrics/scheduler.h"
#include "i3block/controller.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace lyrics {

/// Core application orchestrator.
/// Manages the main loop: polls for song changes, triggers lyrics fetching,
/// and runs the lyric scheduler.
class App {
public:
    /// Callback type for receiving lyric text updates.
    using LyricListener = std::function<void(const std::string&)>;

    explicit App(const Config& cfg);
    ~App();

    /// Run the main loop. Blocks until context is cancelled.
    /// @param stop_flag  Signal this flag to stop (from signal handler).
    void run(std::atomic<bool>& stop_flag);

    /// Set a listener for lyric updates (for GUI/TUI).
    void set_lyric_listener(LyricListener listener) {
        std::lock_guard<std::mutex> lock(listener_mutex_);
        listener_ = std::move(listener);
    }

private:
    Config config_;
    std::unique_ptr<LyricsProvider> provider_;
    std::unique_ptr<Scheduler> scheduler_;
    I3BlockController i3ctrl_;

    // Current song state
    std::string current_song_;
    std::mutex song_mutex_;

    // Lyric listener
    LyricListener listener_;
    std::mutex listener_mutex_;

    // Flag to skip i3block processing for current song (if i3block not running)
    std::atomic<bool> skip_i3block_for_song_{false};

    // Playing status cache
    bool playing_cache_ = false;
    std::chrono::steady_clock::time_point last_playing_check_;
    std::mutex playing_mutex_;

    /// Check for song changes and update accordingly.
    void check_song(std::atomic<bool>& stop_flag);

    /// Broadcast lyrics to all outputs (listener, /dev/shm/lyrics, i3block).
    void broadcast_lyrics(const std::string& text);

    /// Get cached playing status (updated every 3 seconds).
    bool get_cached_playing_status();

    /// Force update playing status cache.
    bool force_update_playing_status();
};

} // namespace lyrics
