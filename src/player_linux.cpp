#include "player.h"
#include "util/log.h"

#include <array>
#include <cstdio>
#include <memory>
#include <regex>
#include <string>
#include <cstring>

namespace lyrics {
namespace player {

// ── Helper: run command, read stdout ─────────────────────────────

static std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        LOG_DEBUG("popen failed: {}", cmd);
        return {};
    }
    while (fgets(buffer.data(), (int)buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Trim trailing newline/whitespace
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

// ── MPC helpers ──────────────────────────────────────────────────

/// Get MPC status: "playing", "paused", or empty.
static std::string mpc_status() {
    std::string out = exec("mpc status 2>/dev/null");
    if (out.empty()) return {};
    if (out.find("[playing]") != std::string::npos) return "playing";
    if (out.find("[paused]") != std::string::npos) return "paused";
    return "stopped";
}

/// Get current song from MPC.
static std::string mpc_current_song() {
    return exec("mpc current -f '%artist% - %title%' 2>/dev/null");
}

/// Get current play time and total duration from MPC status line.
/// Parses format like "[playing] #1/1   0:15/3:42 (7%)"
static bool mpc_parse_time(const std::string& status, double& current, double& total) {
    // Find pattern like "0:15/3:42" or "1:23:45/2:34:56"
    std::regex re(R"((\d+):(\d+)(?::(\d+))?\/(\d+):(\d+)(?::(\d+))?)");
    std::smatch m;
    if (std::regex_search(status, m, re)) {
        // Current time
        int cur_min = std::stoi(m[1]);
        int cur_sec = std::stoi(m[2]);
        if (m[3].matched) {
            current = (double)(cur_min * 3600 + cur_sec * 60 + std::stoi(m[3]));
        } else {
            current = (double)(cur_min * 60 + cur_sec);
        }
        // Total duration
        int tot_min = std::stoi(m[4]);
        int tot_sec = std::stoi(m[5]);
        if (m[6].matched) {
            total = (double)(tot_min * 3600 + tot_sec * 60 + std::stoi(m[6]));
        } else {
            total = (double)(tot_min * 60 + tot_sec);
        }
        return true;
    }
    return false;
}

// ── Playerctl helpers ────────────────────────────────────────────

static std::string playerctl_current_song() {
    std::string song = exec("playerctl metadata --format '{{artist}} - {{title}}' 2>/dev/null");
    if (song.empty()) return {};
    // Check if playing
    std::string status = exec("playerctl status 2>/dev/null");
    if (status == "Stopped") return {};
    return song;
}

static double playerctl_position() {
    std::string out = exec("playerctl position 2>/dev/null");
    if (out.empty()) return 0.0;
    try { return std::stod(out); } catch (...) { return 0.0; }
}

static double playerctl_duration() {
    std::string out = exec("playerctl metadata mpris:length 2>/dev/null");
    if (out.empty()) return 0.0;
    try {
        // playerctl returns microseconds
        int64_t us = std::stoll(out);
        return (double)us / 1000000.0;
    } catch (...) { return 0.0; }
}

// ── Public API ───────────────────────────────────────────────────

std::string get_current_song() {
    // Try MPC first
    std::string status = mpc_status();
    if (status == "playing") {
        std::string song = mpc_current_song();
        if (!song.empty()) {
            LOG_DEBUG("MPC current song: {}", song);
            return song;
        }
    }

    // Fallback to playerctl
    std::string song = playerctl_current_song();
    if (!song.empty()) {
        LOG_DEBUG("Playerctl current song: {}", song);
        return song;
    }

    LOG_DEBUG("No song found from any player");
    return {};
}

double get_current_play_time() {
    // Try MPC first
    std::string status = mpc_status();
    if (status == "playing" || status == "paused") {
        std::string full = exec("mpc status 2>/dev/null");
        double cur = 0, total = 0;
        if (mpc_parse_time(full, cur, total)) {
            LOG_DEBUG("MPC current time: {}s", cur);
            return cur;
        }
    }

    // Fallback to playerctl
    double pos = playerctl_position();
    LOG_DEBUG("Playerctl current time: {}s", pos);
    return pos;
}

double get_current_duration() {
    std::string status = mpc_status();
    if (status == "playing" || status == "paused") {
        std::string full = exec("mpc status 2>/dev/null");
        double cur = 0, total = 0;
        if (mpc_parse_time(full, cur, total)) {
            LOG_DEBUG("MPC duration: {}s", total);
            return total;
        }
    }

    double dur = playerctl_duration();
    LOG_DEBUG("Playerctl duration: {}s", dur);
    return dur;
}

bool is_playing() {
    std::string status = mpc_status();
    if (status == "playing") return true;
    if (status == "paused") return false;

    std::string out = exec("playerctl status 2>/dev/null");
    return out == "Playing";
}

} // namespace player
} // namespace lyrics
