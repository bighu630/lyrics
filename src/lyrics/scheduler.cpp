#include "lyrics/scheduler.h"
#include "player.h"
#include "util/log.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lyrics {

Scheduler::Scheduler() = default;

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start(std::vector<LyricLine> lines,
                       TimeProvider get_time,
                       LyricCallback callback) {
    stop(); // Stop any previous run

    if (lines.empty()) {
        LOG_WARN("Scheduler: no lines to schedule");
        if (callback) callback("");
        return;
    }

    lines_ = std::move(lines);
    running_.store(true);
    stop_requested_.store(false);

    thread_ = std::thread(&Scheduler::run, this, std::move(get_time), std::move(callback));
}

void Scheduler::stop() {
    stop_requested_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    running_.store(false);
}

int Scheduler::find_index_at_time(const std::vector<LyricLine>& lines, double t) {
    if (lines.empty()) return -1;
    if (t < lines[0].time) return -1;

    int left = 0, right = (int)lines.size() - 1;
    int result = -1;

    while (left <= right) {
        int mid = (left + right) / 2;
        if (lines[mid].time <= t) {
            result = mid;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return result;
}

void Scheduler::run(TimeProvider get_time, LyricCallback callback) {
    int last_index = -2; // Different from -1 to ensure first broadcast
    double base_time = get_time();
    auto start_time = std::chrono::steady_clock::now();
    bool was_playing = player::is_playing();

    LOG_INFO("Scheduler started with {} lines, base_time={}s", lines_.size(), base_time);

    while (!stop_requested_.load()) {
        // Calculate current playback time: base + elapsed
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        double current_time = base_time + elapsed;

        // Lookup time with advance shift
        double lookup_time = current_time + kTimeShift;
        int new_index = find_index_at_time(lines_, lookup_time);

        if (new_index != last_index) {
            if (new_index >= 0 && new_index < (int)lines_.size()) {
                const auto& lyric = lines_[new_index];
                LOG_INFO("Lyric [{}] @ {:.3f}s: {}", new_index, current_time, lyric.text);

                if (callback) {
                    callback(lyric.text);
                }
            } else if (new_index == -1 && current_time + kTimeShift < lines_[0].time) {
                if (last_index != -1 && callback) {
                    callback("♪ 即将开始... ♪");
                }
            }
            last_index = new_index;
        }

        // Check if song ended
        if (!lines_.empty() && current_time > lines_.back().time + kSongEndMargin) {
            LOG_INFO("Song ended (current={:.3f}s > last={:.3f}s)", current_time, lines_.back().time);
            break;
        }

        // Calculate smart sleep duration
        double sleep_duration = kMaxSleep;
        int next_index = new_index + 1;
        if (next_index < (int)lines_.size()) {
            double next_lyric_time = lines_[next_index].time - kTimeShift;
            double time_to_next = next_lyric_time - current_time;
            if (time_to_next > 0.5) {
                sleep_duration = time_to_next - 0.1;
            } else if (time_to_next > 0.1) {
                sleep_duration = time_to_next * 0.5;
            } else {
                sleep_duration = kMinSleep;
            }
        }

        // Clamp sleep duration
        if (sleep_duration < kMinSleep) sleep_duration = kMinSleep;
        if (sleep_duration > kMaxSleep) sleep_duration = kMaxSleep;

        // Every kResyncInterval seconds, resync with actual player time
        if (elapsed > kResyncInterval && std::fmod(elapsed, kResyncInterval) < sleep_duration + 0.1) {
            double actual_time = get_time();
            if (actual_time > 0) {
                double drift = actual_time - current_time;
                if (std::abs(drift) > 0.5) {
                    LOG_INFO("Resyncing: drift={:.3f}s, base={:.3f} -> {:.3f}", drift, base_time, actual_time);
                    base_time = actual_time;
                    start_time = std::chrono::steady_clock::now();
                    // Recalculate current_time after resync
                    current_time = actual_time;
                }
            }
        }

        // Sleep (with cancellation check)
        if (sleep_duration > 0) {
            // Break sleep into smaller chunks for responsive cancellation
            const double chunk = 0.05; // 50ms chunks
            double remaining = sleep_duration;
            while (remaining > 0 && !stop_requested_.load()) {
                auto sleep_for = std::min(remaining, chunk);
                std::this_thread::sleep_for(std::chrono::duration<double>(sleep_for));
                remaining -= sleep_for;
            }
        }

        // Check player status periodically
        if (elapsed > 0 && std::fmod(elapsed, 3.0) < 0.1) {
            bool now_playing = player::is_playing();
            if (was_playing && !now_playing) {
                // Just paused - wait for resume
                LOG_INFO("Player paused, waiting...");
                bool resumed = false;
                while (!stop_requested_.load() && !resumed) {
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    if (player::is_playing()) {
                        LOG_INFO("Player resumed, resyncing");
                        base_time = get_time();
                        start_time = std::chrono::steady_clock::now();
                        was_playing = true;
                        resumed = true;
                    }
                }
                if (resumed) continue;
            }
            was_playing = now_playing;
        }
    }

    LOG_INFO("Scheduler stopped");
    running_.store(false);
}

} // namespace lyrics
