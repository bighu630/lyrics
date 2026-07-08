#pragma once
#include <string>
#include "util/http.h"

namespace lyrics {

/// Client for lrcmux.dev API.
/// Provides lyrics lookup by artist and title.
/// Returns raw LRC format text directly (no JSON parsing needed).
class LrcmuxClient {
public:
    LrcmuxClient();

    /// Get provider name ("Lrcmux").
    std::string name() const;

    /// Look up lyrics for a song.
    /// @param title      Song title
    /// @param artist     Artist name
    /// @param duration   Song duration in seconds (unused, kept for interface consistency)
    /// @return LRC lyrics text, or empty string on failure
    std::string get_lyrics(const std::string& title,
                           const std::string& artist,
                           double duration);

private:
    HttpClient http_;
};

} // namespace lyrics
