#pragma once
#include <string>
#include "util/http.h"

namespace lyrics {

/// Client for LRCLib API (https://lrclib.net).
/// Provides synchronized lyrics search by track name, artist, and duration.
class LRCLibClient {
public:
    LRCLibClient();
    
    /// Get provider name ("LRCLib").
    std::string name() const;
    
    /// Search for a song and get its lyrics.
    /// @param title      Song title
    /// @param artist     Artist name
    /// @param duration   Song duration in seconds (0 if unknown)
    /// @return LRC lyrics text, or empty string on failure
    std::string get_lyrics(const std::string& title, const std::string& artist, double duration);
    
private:
    HttpClient http_;
    
    /// Find best matching result from search results JSON.
    std::string find_best_match(const std::string& json_response, 
                                 const std::string& target_title,
                                 const std::string& target_artist,
                                 int target_duration);
};

} // namespace lyrics
