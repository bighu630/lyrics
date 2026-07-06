#include "lyrics/provider.h"
#include "ai/gemini.h"
#include "ai/openai.h"
#include "util/log.h"
#include "util/json.h"

#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

namespace lyrics {

// ── Constructor ─────────────────────────────────────────────────────

LyricsProvider::LyricsProvider(const std::string& cache_dir,
                               const std::string& ai_module,
                               const std::string& ai_key,
                               const std::string& ai_base_url,
                               const std::string& ai_model_name,
                               const std::string& netease_cookie)
    : cache_(cache_dir) {

    // Create AI module based on configuration
    if (ai_module == "gemini") {
        std::string model = ai_model_name.empty() ? "gemini-2.0-flash-exp" : ai_model_name;
        LOG_DEBUG("Creating GeminiClient for LyricsProvider (model: {})", model);
        ai_ = std::make_unique<GeminiClient>(ai_key, model);
    } else if (ai_module == "openai") {
        std::string model = ai_model_name.empty() ? "gpt-3.5-turbo" : ai_model_name;
        LOG_DEBUG("Creating OpenAIClient for LyricsProvider (model: {}, base_url: {})",
                  model, ai_base_url.empty() ? "default" : ai_base_url);
        ai_ = std::make_unique<OpenAIClient>(ai_key, model, ai_base_url);
    } else {
        LOG_WARN("Unknown AI module '{}', falling back to openai (assuming OpenAI-compatible API)", ai_module);
        std::string model = ai_model_name.empty() ? "gpt-3.5-turbo" : ai_model_name;
        ai_ = std::make_unique<OpenAIClient>(ai_key, model, ai_base_url);
    }

    // Create MusicManager and register providers
    music_mgr_ = std::make_unique<MusicManager>();
    music_mgr_->add_provider(MusicManager::ProviderType::LRCLib);
    music_mgr_->add_provider(MusicManager::ProviderType::Netease, netease_cookie);

    LOG_INFO("LyricsProvider initialized (AI: {}, model: {}, providers: {}, cache: {})",
             ai_module, ai_model_name.empty() ? "(default)" : ai_model_name,
             music_mgr_->provider_count(), cache_dir);
}

// ── get_lyrics ──────────────────────────────────────────────────────

std::string LyricsProvider::get_lyrics(const std::string& song_identifier, double duration) {
    LOG_INFO("get_lyrics: '{}' (duration={}s)", song_identifier, duration);

    if (song_identifier.empty()) {
        LOG_WARN("get_lyrics called with empty identifier");
        return "Empty song identifier.";
    }

    // ── Step 1: Check AI result cache ──────────────────────────────
    std::string ai_json = cache_.get_ai_cache(song_identifier);

    if (ai_json.empty()) {
        // ── Step 1b: Cache miss — call AI with retries ─────────────
        LOG_DEBUG("AI cache MISS for '{}', calling AI to identify song", song_identifier);

        std::string prompt = AiInterface::format_query_song(song_identifier);

        const int max_retries = 3;
        const auto retry_delay = std::chrono::seconds(1);

        std::string raw_response;
        for (int attempt = 1; attempt <= max_retries; ++attempt) {
            LOG_DEBUG("AI attempt {}/{} for '{}'", attempt, max_retries, song_identifier);
            raw_response = ai_->handle_text(prompt);

            if (!raw_response.empty()) {
                LOG_DEBUG("AI attempt {}/{} succeeded ({} chars)",
                          attempt, max_retries, raw_response.size());
                break;
            }

            LOG_WARN("AI attempt {}/{} returned empty response for '{}'",
                     attempt, max_retries, song_identifier);
            if (attempt < max_retries) {
                std::this_thread::sleep_for(retry_delay);
            }
        }

        if (raw_response.empty()) {
            LOG_ERROR("AI failed after {} retries for '{}'", max_retries, song_identifier);
            return "Failed to identify song: AI returned empty response after "
                   + std::to_string(max_retries) + " attempts.";
        }

        // Extract JSON from the AI response
        ai_json = AiInterface::extract_song_json(raw_response);

        if (ai_json.empty()) {
            LOG_ERROR("No JSON object found in AI response for '{}': '{}'",
                      song_identifier, raw_response);
            return "Failed to identify song: AI response did not contain valid JSON.";
        }

        LOG_DEBUG("AI extracted JSON: '{}'", ai_json);

    } else {
        LOG_DEBUG("AI cache HIT for '{}'", song_identifier);
    }

    // ── Step 2: Parse the AI JSON result ───────────────────────────
    JsonDoc doc(ai_json);
    if (!doc.valid() || !doc.root()) {
        LOG_ERROR("Failed to parse AI JSON for '{}': '{}'", song_identifier, ai_json);
        return "Failed to parse song information from AI response.";
    }

    yyjson_val* root = doc.root();

    // Check is_song field (default to true for backward compatibility)
    bool is_song = JsonDoc::get_bool(root, "is_song", true);

    if (!is_song) {
        LOG_INFO("AI determined '{}' is not a song", song_identifier);
        return "'" + song_identifier + "' is not a song.";
    }

    // Save to AI cache (only for valid songs)
    cache_.set_ai_cache(song_identifier, ai_json);
    LOG_DEBUG("AI result cached for '{}'", song_identifier);

    // Extract title and artist
    std::string title = JsonDoc::get_str(root, "title");
    std::string artist = JsonDoc::get_str(root, "artist");

    if (title.empty()) {
        LOG_ERROR("AI response missing 'title' field for '{}': '{}'",
                  song_identifier, ai_json);
        return "Failed to identify song: missing title in AI response.";
    }

    LOG_INFO("Identified: '{}' - '{}' (from '{}')", title, artist, song_identifier);

    // ── Step 3: Check LRC file cache ───────────────────────────────
    std::string lrc_path = cache_.get_lrc_path(title, artist);
    if (!lrc_path.empty()) {
        LOG_DEBUG("LRC cache HIT: '{}'", lrc_path);
        std::ifstream ifs(lrc_path);
        if (ifs) {
            std::stringstream ss;
            ss << ifs.rdbuf();
            std::string lyrics = ss.str();
            LOG_INFO("Loaded {} bytes from LRC cache for '{}' - '{}'",
                     lyrics.size(), title, artist);
            return lyrics;
        } else {
            LOG_WARN("LRC cache path '{}' exists but cannot be read", lrc_path);
        }
    } else {
        LOG_DEBUG("LRC cache MISS for '{}' - '{}'", title, artist);
    }

    // ── Step 4: Fetch from multi-source providers ──────────────────
    LOG_DEBUG("Fetching lyrics from providers for '{}' - '{}' (duration={}s)",
              title, artist, duration);
    std::string lyrics = music_mgr_->get_lyrics(title, artist, duration);

    if (lyrics.empty()) {
        LOG_WARN("All lyrics providers failed for '{}' - '{}'", title, artist);
        return "No lyrics found for '" + title + "' by " + artist + ".";
    }

    // ── Step 5: Save to LRC cache ──────────────────────────────────
    cache_.save_lrc(title, artist, lyrics);
    LOG_INFO("Fetched and cached {} bytes of lyrics for '{}' - '{}'",
             lyrics.size(), title, artist);

    return lyrics;
}

} // namespace lyrics
