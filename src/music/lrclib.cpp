#include "music/lrclib.h"
#include "util/log.h"
#include "util/json.h"

#include <cctype>
#include <climits>

namespace lyrics {

// ── Constructor ──────────────────────────────────────────────────

LRCLibClient::LRCLibClient() {
    http_.set_base_url("https://lrclib.net/api");
    http_.set_timeout(std::chrono::seconds(10));
    http_.set_user_agent("lyrics-cpp/1.0");
}

// ── name ─────────────────────────────────────────────────────────

std::string LRCLibClient::name() const {
    return "LRCLib";
}

// ── get_lyrics ───────────────────────────────────────────────────

std::string LRCLibClient::get_lyrics(const std::string& title,
                                      const std::string& artist,
                                      double duration) {
    LOG_DEBUG("LRCLib: searching for title='{}' artist='{}' duration={}",
              title, artist, duration);

    auto resp = http_.get("/search", {{"track_name", title}, {"artist_name", artist}});

    if (!resp.ok()) {
        LOG_WARN("LRCLib search failed: status={} error={}",
                 resp.status_code, resp.error);
        return {};
    }
    if (resp.body == "[]"){
    auto resp = http_.get("/search", {{"track_name", title}});

    if (!resp.ok()) {
        LOG_WARN("LRCLib search failed: status={} error={}",
                 resp.status_code, resp.error);
        return {};
    }

    }

    int target_duration = static_cast<int>(duration);
    return find_best_match(resp.body, title, artist, target_duration);
}

// ── find_best_match ──────────────────────────────────────────────

std::string LRCLibClient::find_best_match(const std::string& json_response,
                                           const std::string& target_title,
                                           const std::string& target_artist,
                                           int target_duration) {
    JsonDoc doc(json_response);
    auto* root = doc.root();
    if (!root || !yyjson_is_arr(root)) {
        LOG_WARN("LRCLib: invalid JSON response (expected array)");
        return {};
    }

    // Case-insensitive string comparison helper.
    auto ieq = [](const std::string& a, const std::string& b) -> bool {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    };

    struct Candidate {
        std::string syncedLyrics;
        std::string plainLyrics;
        int score = 0;
        int durationDiff = INT_MAX;
    };

    Candidate best;

    size_t idx, max;
    yyjson_val* item;
    yyjson_arr_foreach(root, idx, max, item) {
        if (!yyjson_is_obj(item)) continue;

        std::string trackName = JsonDoc::get_str(item, "trackName");
        std::string artistName = JsonDoc::get_str(item, "artistName");
        int itemDuration = static_cast<int>(JsonDoc::get_int(item, "duration", 0));

        // Score the match:
        //   3 = exact title + exact artist
        //   2 = exact title only
        //   1 = partial / API-returned (no strong match)
        int score = 1;
        if (ieq(trackName, target_title)) {
            if (ieq(artistName, target_artist)) {
                score = 3;
            } else {
                score = 2;
            }
        }

        int durDiff = std::abs(itemDuration - target_duration);

        // Prefer higher score; on tie, prefer closer duration.
        bool better = false;
        if (score > best.score) {
            better = true;
        } else if (score == best.score && durDiff < best.durationDiff) {
            better = true;
        }

        if (better) {
            best.score = score;
            best.durationDiff = durDiff;
            best.syncedLyrics = JsonDoc::get_str(item, "syncedLyrics");
            best.plainLyrics = JsonDoc::get_str(item, "plainLyrics");
        }
    }

    // Prefer synced lyrics, fall back to plain lyrics.
    if (!best.syncedLyrics.empty()) {
        LOG_INFO("LRCLib: found lyrics (score={}, synced=true)", best.score);
        return best.syncedLyrics;
    }
    if (!best.plainLyrics.empty()) {
        LOG_INFO("LRCLib: found lyrics (score={}, synced=false)", best.score);
        return best.plainLyrics;
    }

    LOG_WARN("LRCLib: no matching lyrics found");
    return {};
}

} // namespace lyrics
