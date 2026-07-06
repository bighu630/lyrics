#pragma once

#include <string>
#include <vector>

namespace lyrics {

/// Single LRC lyric line.
struct LyricLine {
    double time = 0.0;  ///< Timestamp in seconds
    std::string text;   ///< Lyric text
};

/// Parse LRC format lyrics.
/// Supports [mm:ss.xxx] and [mm:ss.xx] formats.
std::vector<LyricLine> parse_lrc(const std::string& lrc_content);

} // namespace lyrics
