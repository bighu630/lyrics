#include "app.h"
#include "player.h"
#include "util/log.h"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace lyrics {

App::App(const Config& cfg)
    : config_(cfg)
    , provider_(std::make_unique<LyricsProvider>(
          cfg.cache_dir, cfg.ai_module_name, cfg.ai_api_key,
          cfg.ai_base_url, cfg.ai_model_name, cfg.netease_cookie))
    , scheduler_(std::make_unique<Scheduler>()) {

    LOG_INFO("App created (AI: {}, model: {}, cache: {}, check_interval: {}s)",
             cfg.ai_module_name,
             cfg.ai_model_name.empty() ? "(default)" : cfg.ai_model_name,
             cfg.cache_dir, cfg.parsed_check_interval.count());
}

App::~App() {
    scheduler_->stop();
}

void App::run(std::atomic<bool>& stop_flag) {
    // Ensure cache directory exists
    std::error_code ec;
    fs::create_directories(config_.cache_dir, ec);
    if (ec) {
        LOG_WARN("Failed to create cache dir '{}': {}", config_.cache_dir, ec.message());
    }

    // Start i3block controller
    i3ctrl_.start();

    // Initialize playing status
    force_update_playing_status();

    LOG_INFO("Main loop started (check interval: {}s)", config_.parsed_check_interval.count());

    while (!stop_flag.load()) {
        check_song(stop_flag);

        // Sleep for check interval (in smaller chunks for responsive shutdown)
        auto interval = config_.parsed_check_interval;
        auto start = std::chrono::steady_clock::now();

        while (!stop_flag.load()) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= interval) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    scheduler_->stop();
    i3ctrl_.stop();
    LOG_INFO("App stopped");
}

void App::broadcast_lyrics(const std::string& text) {
    if (text.empty()) return;

    std::string output = text;
    // Ensure newline at end
    if (output.back() != '\n') output += '\n';

    // 1. Listener (GUI/TUI)
    {
        std::lock_guard<std::mutex> lock(listener_mutex_);
        if (listener_) {
            listener_(text);
        }
    }

    // 2. Shared memory — always write (for Waybar/TUI direct read)
    i3ctrl_.write_to_shm(output);

    // 3. i3block signal — send if i3block is running (send_signal55 checks pid internally)
    i3ctrl_.send_signal55();
}

void App::check_song(std::atomic<bool>& stop_flag) {
    // Get current song from player
    std::string song_identifier = player::get_current_song();

    // Skip if identifier starts with " - " (format issue)
    if (song_identifier.size() > 3 && song_identifier.substr(0, 3) == " - ") {
        return;
    }

    if (song_identifier.empty()) {
        {
            std::lock_guard<std::mutex> lock(song_mutex_);
            current_song_.clear();
        }
        broadcast_lyrics("No music playing...");
        return;
    }

    // Bug 2: Track playing state to detect stale metadata when resuming from pause
    static bool was_playing = true;
    bool now_playing = player::is_playing();
    bool just_resumed = !was_playing && now_playing;
    was_playing = now_playing;

    if (just_resumed && !current_song_.empty()) {
        LOG_INFO("Player resumed, forcing song re-check (handles stale metadata)");
        std::lock_guard<std::mutex> lock(song_mutex_);
        current_song_.clear();
    }

    // Check if song changed
    {
        std::lock_guard<std::mutex> lock(song_mutex_);
        if (song_identifier == current_song_) {
            return; // Same song, skip
        }
        LOG_INFO("─────────────────────────────────────────────────────");
        LOG_INFO("New song detected: '{}'", song_identifier);
        current_song_ = song_identifier;

    }

    // Broadcast the identifier immediately
    broadcast_lyrics(song_identifier);

    // Check duration
    double duration = player::get_current_duration();
    if (duration > 360.0) { // Skip songs longer than 6 minutes
        LOG_INFO("Song too long ({}s > 360s), skipping lyrics fetch", duration);
        broadcast_lyrics("歌曲长度过长");
        return;
    }

    // Stop previous scheduler
    scheduler_->stop();

    // Get lyrics
    auto lyrics_text = provider_->get_lyrics(song_identifier, duration);

    if (lyrics_text.empty()) {
        LOG_WARN("No lyrics returned for '{}'", song_identifier);
        return;
    }

    // Parse and start scheduler
    auto lines = parse_lrc(lyrics_text);
    auto line_count = lines.size();

    if (lines.empty()) {
        // No parseable LRC lines - broadcast as raw text
        LOG_WARN("No LRC lines parsed, broadcasting raw text");
        broadcast_lyrics(lyrics_text);
        return;
    }

    // Start the scheduler
    scheduler_->start(std::move(lines),
                      player::get_current_play_time,
                      [this](const std::string& lyric) {
                          this->broadcast_lyrics(lyric);
                      });

    LOG_INFO("Lyrics scheduler started ({} lines)", line_count);
}

bool App::get_cached_playing_status() {
    constexpr auto kCacheDuration = std::chrono::seconds(3);
    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(playing_mutex_);
    if (now - last_playing_check_ >= kCacheDuration) {
        playing_cache_ = player::is_playing();
        last_playing_check_ = now;
        LOG_DEBUG("Updated playing status: {}", playing_cache_);
    }
    return playing_cache_;
}

bool App::force_update_playing_status() {
    std::lock_guard<std::mutex> lock(playing_mutex_);
    playing_cache_ = player::is_playing();
    last_playing_check_ = std::chrono::steady_clock::now();
    return playing_cache_;
}

} // namespace lyrics
