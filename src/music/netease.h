#pragma once
#include <string>
#include "util/http.h"

namespace lyrics {

/// Client for Netease Cloud Music API.
/// Provides lyrics search by song title and artist.
class NeteaseClient {
public:
    explicit NeteaseClient(const std::string& cookie = "");

    /// Get provider name ("Netease").
    std::string name() const;

    /// Search for a song and get its lyrics.
    /// @param title      Song title
    /// @param artist     Artist name
    /// @return LRC lyrics text, or empty string on failure
    std::string get_lyrics(const std::string& title, const std::string& artist);

private:
    HttpClient http_;
    std::string cookie_;

    /// Search for song ID by title.
    std::string search_song(const std::string& title, const std::string& artist);

    /// Get lyrics by song ID.
    std::string get_lyrics_by_id(const std::string& song_id);
};

} // namespace lyrics
