#include "netease.h"

#include "util/http.h"
#include "util/json.h"
#include "util/log.h"

#include <algorithm>
#include <cctype>
#include <chrono>

namespace lyrics {

NeteaseClient::NeteaseClient(const std::string& cookie)
    : cookie_(cookie) {
    http_.set_user_agent("lyrics-cpp/1.0");
    http_.set_timeout(std::chrono::seconds(5));
}

std::string NeteaseClient::name() const {
    return "Netease";
}

std::string NeteaseClient::search_song(const std::string& title, const std::string& artist) {
    // Build search URL
    std::string url = "https://music.163.com/api/search/get/web"
                      "?csrf_token=hlpretag&hlposttag=&s=" + title +
                      "&type=1&limit=100";

    // Set cookie if provided
    if (!cookie_.empty()) {
        http_.set_cookie(cookie_);
    }

    LOG_DEBUG("Netease searching: title='{}' artist='{}'", title, artist);

    auto resp = http_.get(url);
    if (!resp.ok()) {
        LOG_WARN("Netease search request failed [{}]: {}", resp.status_code, resp.body);
        return {};
    }

    // Parse JSON response
    JsonDoc doc(resp.body);
    if (!doc.valid()) {
        LOG_WARN("Netease search returned invalid JSON");
        return {};
    }

    yyjson_val* root = doc.root();
    if (!root || !yyjson_is_obj(root)) {
        LOG_WARN("Netease search response root is not an object");
        return {};
    }

    yyjson_val* result_val = yyjson_obj_get(root, "result");
    if (!result_val || !yyjson_is_obj(result_val)) {
        LOG_WARN("Netease search response missing 'result'");
        return {};
    }

    yyjson_val* songs = yyjson_obj_get(result_val, "songs");
    if (!songs || !yyjson_is_arr(songs)) {
        LOG_WARN("Netease search response missing 'result.songs' array");
        return {};
    }

    // Helper: lowercase string for case-insensitive comparison
    auto to_lower = [](const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    };

    std::string title_lower = to_lower(title);
    std::string artist_lower = to_lower(artist);

    // Iterate through songs to find best match
    yyjson_val* song;
    yyjson_val* first_title_match_song = nullptr;

    yyjson_arr_iter iter = yyjson_arr_iter_with(songs);
    while ((song = yyjson_arr_iter_next(&iter))) {
        if (!yyjson_is_obj(song)) continue;

        std::string song_name = JsonDoc::get_str(song, "name");
        std::string song_name_lower = to_lower(song_name);

        // Check if name contains target title
        if (song_name_lower.find(title_lower) == std::string::npos) {
            continue;
        }

        // Remember first song that matches title
        if (first_title_match_song == nullptr) {
            first_title_match_song = song;
        }

        // Get artists array
        yyjson_val* artists = yyjson_obj_get(song, "artists");
        if (!artists || !yyjson_is_arr(artists)) continue;

        yyjson_val* first_artist = yyjson_arr_get(artists, 0);
        if (!first_artist || !yyjson_is_obj(first_artist)) continue;

        std::string artist_name = JsonDoc::get_str(first_artist, "name");
        std::string artist_name_lower = to_lower(artist_name);

        // Exact match: both title and artist match
        if (artist_name_lower.find(artist_lower) != std::string::npos) {
            int64_t song_id = JsonDoc::get_int(song, "id");
            LOG_INFO("Netease found exact match: id={} name='{}' artist='{}'",
                     song_id, song_name, artist_name);
            return std::to_string(song_id);
        }
    }

    // No exact match found; return first title match if any
    if (first_title_match_song) {
        int64_t song_id = JsonDoc::get_int(first_title_match_song, "id");
        std::string song_name = JsonDoc::get_str(first_title_match_song, "name");

        yyjson_val* artists = yyjson_obj_get(first_title_match_song, "artists");
        std::string artist_name;
        if (artists && yyjson_is_arr(artists)) {
            yyjson_val* first_artist = yyjson_arr_get(artists, 0);
            if (first_artist && yyjson_is_obj(first_artist)) {
                artist_name = JsonDoc::get_str(first_artist, "name");
            }
        }

        LOG_WARN("Netease no exact match, using first title match: id={} name='{}' artist='{}'",
                 song_id, song_name, artist_name);
        return std::to_string(song_id);
    }

    LOG_WARN("Netease no matching song found for title='{}' artist='{}'", title, artist);
    return {};
}

std::string NeteaseClient::get_lyrics_by_id(const std::string& song_id) {
    std::string url = "http://music.163.com/api/song/lyric?os=pc&id=" + song_id +
                      "&lv=-1&kv=-1&tv=-1";

    LOG_DEBUG("Netease fetching lyrics for song id={}", song_id);

    auto resp = http_.get(url);
    if (!resp.ok()) {
        LOG_WARN("Netease lyrics request failed [{}]: {}", resp.status_code, resp.body);
        return {};
    }

    // Parse JSON response
    JsonDoc doc(resp.body);
    if (!doc.valid()) {
        LOG_WARN("Netease lyrics returned invalid JSON");
        return {};
    }

    yyjson_val* root = doc.root();
    if (!root || !yyjson_is_obj(root)) {
        LOG_WARN("Netease lyrics response root is not an object");
        return {};
    }

    yyjson_val* lrc = yyjson_obj_get(root, "lrc");
    if (!lrc || !yyjson_is_obj(lrc)) {
        LOG_WARN("Netease lyrics response missing 'lrc' object");
        return {};
    }

    std::string lyric = JsonDoc::get_str(lrc, "lyric");
    if (lyric.empty()) {
        LOG_WARN("Netease lyrics response has empty 'lrc.lyric'");
        return {};
    }

    LOG_INFO("Netease got lyrics for song id={} ({} bytes)", song_id, lyric.size());
    return lyric;
}

std::string NeteaseClient::get_lyrics(const std::string& title, const std::string& artist) {
    LOG_INFO("Netease get_lyrics: title='{}' artist='{}'", title, artist);

    std::string song_id = search_song(title, artist);
    if (song_id.empty()) {
        LOG_WARN("Netease could not find song: title='{}' artist='{}'", title, artist);
        return {};
    }

    return get_lyrics_by_id(song_id);
}

} // namespace lyrics
