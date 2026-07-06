#pragma once
#include <string>
#include <memory>
#include "ai/ai_interface.h"
#include "music/music_manager.h"
#include "cache/cache.h"

namespace lyrics {

/// Lyrics Provider - orchestrates AI song recognition + multi-source lyrics fetching.
/// Flow:
/// 1. AI identifies song title/artist from media identifier string
/// 2. Cache check for LRC (by title-artist filename)
/// 3. Multi-source LRC fetch (LRCLib -> Netease via MusicManager)
/// 4. Cache the result
class LyricsProvider {
public:
    /// @param cache_dir      Directory for LRC file cache
    /// @param ai_module      AI module name ("gemini" or "openai")
    /// @param ai_key         API key for the AI service
    /// @param ai_base_url    Base URL for OpenAI-compatible APIs
    /// @param ai_model_name  Model name (e.g., "gpt-4o", "qwen3-max"); empty for default
    /// @param netease_cookie Cookie for Netease API
    LyricsProvider(const std::string& cache_dir,
                   const std::string& ai_module,
                   const std::string& ai_key,
                   const std::string& ai_base_url,
                   const std::string& ai_model_name,
                   const std::string& netease_cookie);

    ~LyricsProvider() = default;

    /// Get lyrics for a song identifier (e.g., "Artist - Title").
    /// @param song_identifier  Raw media title string from player
    /// @param duration         Song duration in seconds
    /// @return LRC lyrics text, or error description string
    std::string get_lyrics(const std::string& song_identifier, double duration);

private:
    std::unique_ptr<AiInterface> ai_;
    std::unique_ptr<MusicManager> music_mgr_;
    Cache cache_;
};

} // namespace lyrics
