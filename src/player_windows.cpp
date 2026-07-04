#ifdef _WIN32

#include "player.h"
#include "util/json.h"
#include "util/log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace lyrics {
namespace player {

namespace {

// ── State (mutex-protected) ──────────────────────────────────────

std::mutex g_mutex;
std::string g_song_identifier;
double g_position = 0.0;
double g_duration = 0.0;
std::string g_status;
std::atomic<bool> g_shutdown{false};

// ── SMTC helper reader thread ────────────────────────────────────

void helper_read_thread() {
    int attempt = 0;

    while (!g_shutdown.load()) {
        // Launch smtc-helper.exe
        FILE* pipe = _popen("smtc-helper.exe", "r");
        if (!pipe) {
            LOG_ERROR("Failed to launch smtc-helper.exe");
            int delay = 1 << attempt; // 1, 2, 4, 8, ...
            if (delay > 30)
                delay = 30;
            std::this_thread::sleep_for(std::chrono::seconds(delay));
            attempt++;
            continue;
        }

        // Successful launch — reset backoff
        attempt = 0;

        char buf[4096];
        while (!g_shutdown.load() && fgets(buf, sizeof(buf), pipe)) {
            std::string line(buf);

            // Trim trailing newline(s)
            while (!line.empty() &&
                   (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            if (line.empty())
                continue;

            // Parse JSON line
            JsonDoc doc(line);
            if (!doc.valid() || !doc.root())
                continue;

            yyjson_val* root = doc.root();
            std::string type = JsonDoc::get_str(root, "type");

            if (type == "media") {
                std::string title  = JsonDoc::get_str(root, "title");
                std::string artist = JsonDoc::get_str(root, "artist");
                std::string status = JsonDoc::get_str(root, "status");

                yyjson_val* pos_val = yyjson_obj_get(root, "position");
                double position = pos_val ? yyjson_get_real(pos_val) : 0.0;

                yyjson_val* dur_val = yyjson_obj_get(root, "duration");
                double duration = dur_val ? yyjson_get_real(dur_val) : 0.0;

                // Build song identifier
                std::string song;
                if (!artist.empty() && !title.empty()) {
                    song = artist + " - " + title;
                } else if (!title.empty()) {
                    song = title;
                }

                // Publish state under lock
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_song_identifier = song;
                    g_position        = position;
                    g_duration        = duration;
                    g_status          = status;
                }

                LOG_DEBUG("SMTC: {} | {:.1f}s/{:.1f}s | {}", song, position,
                          duration, status);

            } else if (type == "error") {
                std::string msg   = JsonDoc::get_str(root, "message");
                yyjson_val* fval = yyjson_obj_get(root, "fatal");
                bool fatal = fval ? yyjson_get_bool(fval) : false;

                LOG_ERROR("SMTC helper error: {} (fatal={})", msg, fatal);

                if (fatal) {
                    // Fatal error — break out to restart helper
                    break;
                }

            } else if (type == "ready") {
                LOG_DEBUG("SMTC helper ready");
            }
        }

        // Clean up
        _pclose(pipe);

        if (g_shutdown.load())
            break;

        // Exponential backoff before restart
        int delay = 1 << attempt;
        if (delay > 30)
            delay = 30;
        LOG_DEBUG("SMTC helper disconnected, restarting in {}s", delay);
        std::this_thread::sleep_for(std::chrono::seconds(delay));
        attempt++;
    }
}

// ── Startup ──────────────────────────────────────────────────────

/// Starts the SMTC reader thread on first call to any public API.
std::thread& ensure_thread() {
    static std::thread t(helper_read_thread);
    return t;
}

} // anonymous namespace

// ── Public API (player.h interface) ──────────────────────────────

std::string get_current_song() {
    (void)ensure_thread(); // trigger lazy thread start
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_song_identifier;
}

double get_current_play_time() {
    (void)ensure_thread();
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_position;
}

double get_current_duration() {
    (void)ensure_thread();
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_duration;
}

bool is_playing() {
    (void)ensure_thread();
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_status == "playing";
}

} // namespace player
} // namespace lyrics

#endif // _WIN32
