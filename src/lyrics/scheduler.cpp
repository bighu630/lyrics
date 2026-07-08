#include "lyrics/scheduler.h"
#include "util/log.h"

#include <algorithm>
#include <utility>

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

    LOG_INFO("Scheduler started with {} lines", lines_.size());

    while (!stop_requested_.load()) {
        // Use get_time() directly as the primary time source.
        // During pause, get_time() returns the paused position (time stops advancing),
        // so lyrics naturally stay on the current line without any special pause handling.
        double current_time = get_time();

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
    }

    LOG_INFO("Scheduler stopped");
    running_.store(false);
}

} // namespace lyrics
