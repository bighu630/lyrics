#include "cache/cache.h"
#include "util/log.h"

#include <fstream>
#include <regex>
#include <sstream>

namespace lyrics {

Cache::Cache(const std::string& dir)
    : cache_dir_(dir)
    , ai_cache_file_(dir + "/music_cache.list") {

    // Ensure cache directory exists
    std::error_code ec;
    std::filesystem::create_directories(cache_dir_, ec);
    if (ec) {
        LOG_WARN("Failed to create cache directory '{}': {}", cache_dir_, ec.message());
    }

    load_ai_cache();
}

// ── AI Cache ─────────────────────────────────────────────────────

void Cache::load_ai_cache() {
    std::ifstream ifs(ai_cache_file_);
    if (!ifs) {
        // File doesn't exist yet - create it
        std::ofstream ofs(ai_cache_file_);
        return;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        // Format: "key => value"
        auto pos = line.find(" => ");
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 4);
        ai_cache_[key] = val;
    }

    LOG_DEBUG("Loaded {} AI cache entries from '{}'", ai_cache_.size(), ai_cache_file_);
}

void Cache::append_ai_cache(const std::string& key, const std::string& value) {
    std::ofstream ofs(ai_cache_file_, std::ios::app);
    if (!ofs) {
        LOG_WARN("Cannot write to AI cache file '{}'", ai_cache_file_);
        return;
    }
    ofs << key << " => " << value << "\n";
}

std::string Cache::get_ai_cache(const std::string& key) {
    std::shared_lock lock(mutex_);
    auto it = ai_cache_.find(key);
    if (it != ai_cache_.end()) {
        LOG_DEBUG("AI cache HIT for '{}'", key);
        return it->second;
    }
    LOG_DEBUG("AI cache MISS for '{}'", key);
    return {};
}

void Cache::set_ai_cache(const std::string& key, const std::string& value) {
    {
        std::unique_lock lock(mutex_);
        if (ai_cache_.find(key) != ai_cache_.end()) {
            return; // already cached
        }
        ai_cache_[key] = value;
    }
    append_ai_cache(key, value);
}

// ── LRC Cache ────────────────────────────────────────────────────

std::string Cache::sanitize(const std::string& name) {
    static const std::regex unsafe(R"([\\/:*?"<>|])");
    return std::regex_replace(name, unsafe, "-");
}

std::string Cache::get_lrc_path(const std::string& title, const std::string& artist) const {
    std::string filename = sanitize(title + "-" + artist) + ".lrc";
    std::string path = cache_dir_ + "/" + filename;

    if (std::filesystem::exists(path)) {
        LOG_DEBUG("LRC cache HIT: '{}'", path);
        return path;
    }
    LOG_DEBUG("LRC cache MISS: '{}'", path);
    return {};
}

std::string Cache::save_lrc(const std::string& title, const std::string& artist,
                             const std::string& lyrics) {
    std::string filename = sanitize(title + "-" + artist) + ".lrc";
    std::string path = cache_dir_ + "/" + filename;

    std::ofstream ofs(path);
    if (!ofs) {
        LOG_WARN("Failed to write LRC cache file '{}'", path);
        return path;
    }
    ofs << lyrics;
    LOG_DEBUG("Saved LRC cache: '{}' ({} bytes)", path, lyrics.size());
    return path;
}

} // namespace lyrics
