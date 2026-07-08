#include "player.h"
#include "util/exec.h"
#include "util/log.h"

#include <regex>
#include <string>

namespace lyrics {
namespace player {

// ── MPC helpers ──────────────────────────────────────────────────

/// Get MPC status: "playing", "paused", or empty.
static std::string mpc_status() {
    std::string out = util::exec("mpc status 2>/dev/null");
    if (out.empty()) return {};
    if (out.find("[playing]") != std::string::npos) return "playing";
    if (out.find("[paused]") != std::string::npos) return "paused";
    return "stopped";
}

/// Get current song from MPC.
static std::string mpc_current_song() {
    return util::exec("mpc current -f '%artist% - %title%' 2>/dev/null");
}

/// Parse current play time and total duration from MPC status line.
/// Format: "[playing] #1/1   0:15/3:42 (7%)" or "1:23:45/2:34:56".
static bool mpc_parse_time(const std::string& status, double& current, double& total) {
    static const std::regex re(R"((\d+):(\d+)(?::(\d+))?\/(\d+):(\d+)(?::(\d+))?)");
    std::smatch m;
    if (std::regex_search(status, m, re)) {
        int cur_min = std::stoi(m[1]);
        int cur_sec = std::stoi(m[2]);
        if (m[3].matched) {
            current = static_cast<double>(cur_min * 3600 + cur_sec * 60 + std::stoi(m[3]));
        } else {
            current = static_cast<double>(cur_min * 60 + cur_sec);
        }
        int tot_min = std::stoi(m[4]);
        int tot_sec = std::stoi(m[5]);
        if (m[6].matched) {
            total = static_cast<double>(tot_min * 3600 + tot_sec * 60 + std::stoi(m[6]));
        } else {
            total = static_cast<double>(tot_min * 60 + tot_sec);
        }
        return true;
    }
    return false;
}

// ── Playerctl helpers ────────────────────────────────────────────

static std::string playerctl_current_song() {
    std::string status = util::exec("playerctl status 2>/dev/null");
    // Only return metadata for active playback states.
    // "Playing" and "Paused" both have valid metadata;
    // "Stopped" returns stale data. Track-switch while paused
    // is handled by App::check_song() playing-state detection.
    if (status != "Playing" && status != "Paused") return {};
    std::string song = util::exec("playerctl metadata --format '{{artist}} - {{title}}' 2>/dev/null");
    return song;
}

static double playerctl_position() {
    std::string out = util::exec("playerctl position 2>/dev/null");
    if (out.empty()) return 0.0;
    try { return std::stod(out); } catch (...) { return 0.0; }
}

static double playerctl_duration() {
    std::string out = util::exec("playerctl metadata mpris:length 2>/dev/null");
    if (out.empty()) return 0.0;
    try {
        int64_t us = std::stoll(out);
        return static_cast<double>(us) / 1000000.0;
    } catch (...) { return 0.0; }
}

// ── Public API ───────────────────────────────────────────────────

std::string get_current_song() {
    std::string status = mpc_status();
    if (status == "playing") {
        std::string song = mpc_current_song();
        if (!song.empty()) {
            LOG_DEBUG("MPC current song: {}", song);
            return song;
        }
    }

    std::string song = playerctl_current_song();
    if (!song.empty()) {
        LOG_DEBUG("Playerctl current song: {}", song);
        return song;
    }

    LOG_DEBUG("No song found from any player");
    return {};
}

double get_current_play_time() {
    std::string status = mpc_status();
    if (status == "playing") {
        std::string full = util::exec("mpc status 2>/dev/null");
        double cur = 0, total = 0;
        if (mpc_parse_time(full, cur, total)) {
            LOG_DEBUG("MPC current time: {}s", cur);
            return cur;
        }
    }

    double pos = playerctl_position();
    LOG_DEBUG("Playerctl current time: {}s", pos);
    return pos;
}

double get_current_duration() {
    std::string status = mpc_status();
    if (status == "playing") {
        std::string full = util::exec("mpc status 2>/dev/null");
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

    std::string out = util::exec("playerctl status 2>/dev/null");
    return out == "Playing";
}

} // namespace player
} // namespace lyrics
