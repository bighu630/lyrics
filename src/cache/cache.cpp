#include "cache/cache.h"
#include "util/log.h"
#include "util/json.h"

#include <fstream>
#include <regex>
#include <sstream>

namespace lyrics {

Cache::Cache(const std::string& dir)
    : cache_dir_(dir)
    , ai_cache_file_(dir + "/music_cache.jsonl") {

    // Ensure cache directory exists
    std::error_code ec;
    std::filesystem::create_directories(cache_dir_, ec);
    if (ec) {
        LOG_WARN("Failed to create cache directory '{}': {}", cache_dir_, ec.message());
    }

    load_ai_cache();
}

// ── AI Cache (JSON-lines format) ────────────────────────────────

void Cache::load_ai_cache() {
    std::ifstream ifs(ai_cache_file_);
    if (!ifs) {
        // File doesn't exist yet - create it
        std::ofstream ofs(ai_cache_file_);
        return;
    }

    std::string line;
    int loaded = 0;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        // Parse JSON: {"key": "...", "value": "..."}
        JsonDoc doc(line);
        if (!doc.valid() || !doc.root()) continue;
        yyjson_val* root = doc.root();
        std::string key = JsonDoc::get_str(root, "key");
        std::string val = JsonDoc::get_str(root, "value");
        if (!key.empty() && !val.empty()) {
            ai_cache_[key] = val;
            loaded++;
        }
    }

    LOG_DEBUG("Loaded {} AI cache entries from '{}'", loaded, ai_cache_file_);
}

void Cache::append_ai_cache(const std::string& key, const std::string& value) {
    // Build JSON line: {"key":"...","value":"..."}
    // Use JsonDoc for proper JSON escaping
    JsonDoc entry = JsonDoc::make_object();
    yyjson_mut_doc* doc = entry.doc_mut();
    yyjson_mut_val* root = entry.root_mut();
    JsonDoc::set_str(root, doc, "key", key);
    JsonDoc::set_str(root, doc, "value", value);

    std::string json_line = entry.to_string();

    std::ofstream ofs(ai_cache_file_, std::ios::app);
    if (!ofs) {
        LOG_WARN("Cannot write to AI cache file '{}'", ai_cache_file_);
        return;
    }
    ofs << json_line << "\n";
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
