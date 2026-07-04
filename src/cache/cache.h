#pragma once

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <filesystem>

namespace lyrics {

/// Cache manager for both AI results (memory + file) and LRC files.
class Cache {
public:
    explicit Cache(const std::string& cache_dir);

    /// ── AI result cache (key: song identifier, value: AI JSON) ──

    /// Get cached AI result. Returns empty string if not found.
    std::string get_ai_cache(const std::string& key);

    /// Store AI result.
    void set_ai_cache(const std::string& key, const std::string& value);

    /// ── LRC file cache ──────────────────────────────────────────

    /// Get cached LRC file path for a song. Returns empty string if not found.
    std::string get_lrc_path(const std::string& title, const std::string& artist) const;

    /// Save lyrics to LRC cache file. Returns the file path.
    std::string save_lrc(const std::string& title, const std::string& artist,
                         const std::string& lyrics);

    /// Get the cache directory path.
    const std::string& cache_dir() const { return cache_dir_; }

private:
    std::string cache_dir_;
    std::string ai_cache_file_;
    std::unordered_map<std::string, std::string> ai_cache_; // in-memory cache
    mutable std::shared_mutex mutex_;

    /// Load AI cache from disk.
    void load_ai_cache();

    /// Append one entry to the AI cache file.
    void append_ai_cache(const std::string& key, const std::string& value);

    /// Sanitize a filename (remove unsafe characters).
    static std::string sanitize(const std::string& name);
};

} // namespace lyrics
