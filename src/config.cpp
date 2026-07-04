#include "config.h"
#include "util/log.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace lyrics {

// ── Default cache dir ────────────────────────────────────────────

std::string Config::default_cache_dir() {
    const char* xdg_cache = std::getenv("XDG_CACHE_HOME");
    if (xdg_cache && xdg_cache[0]) {
        return std::string(xdg_cache) + "/lyrics";
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.cache/lyrics";
    }
    return "lyrics_cache";
}

// ── Helper: get string from toml table ───────────────────────────

static std::string toml_str(toml_table_t* tbl, const char* key) {
    const char* raw = toml_raw_in(tbl, key);
    if (!raw) return {};
    char* val_str = nullptr;
    if (toml_rtos(raw, &val_str) == 0 && val_str) {
        std::string result(val_str);
        free(val_str);
        return result;
    }
    return {};
}

// ── Parse TOML table into Config ─────────────────────────────────

void Config::parse_toml(toml_table_t* tbl) {
    // [app]
    toml_table_t* app_tbl = toml_table_in(tbl, "app");
    if (app_tbl) {
        auto ci = toml_str(app_tbl, "check_interval");
        if (!ci.empty()) { check_interval_str = ci; }

        auto cd = toml_str(app_tbl, "cache_dir");
        if (!cd.empty()) cache_dir = cd;

        auto ll = toml_str(app_tbl, "log_level");
        if (!ll.empty()) log_level = ll;
    }

    // [ai]
    toml_table_t* ai_tbl = toml_table_in(tbl, "ai");
    if (ai_tbl) {
        auto mn = toml_str(ai_tbl, "module_name");
        if (!mn.empty()) ai_module_name = mn;

        auto ak = toml_str(ai_tbl, "api_key");
        if (!ak.empty()) ai_api_key = ak;

        auto bu = toml_str(ai_tbl, "base_url");
        if (!bu.empty()) ai_base_url = bu;
    }

    // [lrc]
    toml_table_t* lrc_tbl = toml_table_in(tbl, "lrc");
    if (lrc_tbl) {
        auto nc = toml_str(lrc_tbl, "netease_cookie");
        if (!nc.empty()) netease_cookie = nc;
    }

    // Parse check interval
    if (!check_interval_str.empty()) {
        try {
            // Simple parsing: assume numeric + suffix, e.g., "5s"
            char unit = 's';
            double val = 0;
            if (check_interval_str.back() == 's') {
                val = std::stod(check_interval_str.substr(0, check_interval_str.size() - 1));
                unit = 's';
            } else if (check_interval_str.back() == 'm') {
                val = std::stod(check_interval_str.substr(0, check_interval_str.size() - 1));
                unit = 'm';
            } else {
                val = std::stod(check_interval_str);
            }
            if (val > 0) {
                if (unit == 'm') parsed_check_interval = std::chrono::seconds((int)(val * 60));
                else parsed_check_interval = std::chrono::seconds((int)val);
            }
        } catch (...) {
            LOG_WARN("Invalid check_interval '{}', using default 5s", check_interval_str);
            parsed_check_interval = std::chrono::seconds(5);
        }
    }
}

// ── Load ─────────────────────────────────────────────────────────

Config Config::load() {
    // Determine config file path
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::string config_path;
    if (xdg_config && xdg_config[0]) {
        config_path = std::string(xdg_config) + "/lyrics/config.toml";
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            config_path = std::string(home) + "/.config/lyrics/config.toml";
        } else {
            config_path = "config.toml";
        }
    }
    return load_from(config_path);
}

Config Config::load_from(const std::string& path) {
    Config cfg;

    // Set defaults
    cfg.cache_dir = default_cache_dir();

    // Check if config file exists
    if (!fs::exists(path)) {
        LOG_INFO("Config file not found at '{}', using defaults", path);
        return cfg;
    }

    // Read file
    std::ifstream ifs(path);
    if (!ifs) {
        LOG_WARN("Cannot open config file '{}', using defaults", path);
        return cfg;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();

    // Parse TOML (need mutable copy for toml_parse)
    char errbuf[200];
    std::string mutable_content = content;
    toml_table_t* tbl = toml_parse(&mutable_content[0], errbuf, sizeof(errbuf));
    if (!tbl) {
        LOG_WARN("Failed to parse config TOML: {} (using defaults)", errbuf);
        return cfg;
    }

    cfg.parse_toml(tbl);
    toml_free(tbl);

    LOG_INFO("Loaded config from '{}'", path);
    return cfg;
}

} // namespace lyrics
