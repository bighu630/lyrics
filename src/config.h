#pragma once

#include <string>
#include <chrono>
#include "tomlc99/toml.h"

namespace lyrics {

/// Parsed configuration loaded from ~/.config/lyrics/config.toml.
struct Config {
    // ── App section ─────────────────────────────────────────────
    std::string check_interval_str{"5s"};
    std::string cache_dir;
    std::string log_level{"info"};
    std::chrono::seconds parsed_check_interval{5};

    // ── AI section ──────────────────────────────────────────────
    std::string ai_module_name{"gemini"};
    std::string ai_api_key;
    std::string ai_base_url;

    // ── LRC section ─────────────────────────────────────────────
    std::string netease_cookie;

    /// Load configuration from the default path (~/.config/lyrics/config.toml).
    static Config load();

    /// Load from a specific file path.
    static Config load_from(const std::string& path);

    /// Get the default cache directory (~/.cache/lyrics).
    static std::string default_cache_dir();

private:
    void parse_toml(toml_table_t* tbl);
};

} // namespace lyrics
