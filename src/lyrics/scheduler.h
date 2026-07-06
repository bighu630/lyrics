#pragma once

#include "lyrics/lrc_parser.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace lyrics {

/// Callback type for scheduler: receives current lyric text.
using LyricCallback = std::function<void(const std::string&)>;

/// Lyrics scheduler that synchronizes LRC lines with playback position.
///
/// - Uses a base time + elapsed approach for smooth timing
/// - Binary search for current lyric line
/// - 0.1s advance display
/// - Intelligent sleep between checks
/// - Resyncs every 5 seconds
/// - Handles pause/resume
class Scheduler {
public:
    using TimeProvider = std::function<double()>;

    Scheduler();
    ~Scheduler();

    /// Start scheduling. Replaces any running scheduler.
    /// @param lines       Parsed LRC lines
    /// @param get_time    Function that returns current playback time in seconds
    /// @param callback    Function called on each lyric change
    void start(std::vector<LyricLine> lines,
               TimeProvider get_time,
               LyricCallback callback);

    /// Stop the scheduler immediately.
    void stop();

    /// Check if scheduler is running.
    bool running() const { return running_.load(); }

private:
    static constexpr double kTimeShift = 0.1;     // Seconds to show lyric early
    static constexpr double kResyncInterval = 5.0; // Seconds between resyncs
    static constexpr double kSongEndMargin = 5.0;  // Seconds after last line to stop
    static constexpr double kMinSleep = 0.02;      // 20ms minimum sleep
    static constexpr double kMaxSleep = 1.0;       // 1s maximum sleep

    std::vector<LyricLine> lines_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    /// Main scheduler loop.
    void run(TimeProvider get_time, LyricCallback callback);

    /// Find the last lyric line with time <= t.
    /// Uses binary search (O(log n)).
    static int find_index_at_time(const std::vector<LyricLine>& lines, double t);
};

} // namespace lyrics
