#include "lyrics/lrc_parser.h"
#include "util/log.h"

#include <algorithm>
#include <regex>
#include <sstream>

namespace lyrics {

std::vector<LyricLine> parse_lrc(const std::string& lrc_content) {
    std::vector<LyricLine> result;

    // Regex: [mm:ss.xxx]text or [mm:ss.xx]text
    // Group 1: minutes, Group 2: seconds, Group 3: milliseconds (optional), Group 4: text
    static const std::regex re(R"(\[(\d{2}):(\d{2})(?:\.(\d{1,3}))?\](.*))");

    std::istringstream stream(lrc_content);
    std::string line;

    while (std::getline(stream, line)) {
        // A line may have multiple timestamps pointing to the same text
        auto it = line.cbegin();
        std::smatch match;

        while (std::regex_search(it, line.cend(), match, re)) {
            int min = std::stoi(match[1]);
            int sec = std::stoi(match[2]);
            int ms = 0;

            if (match[3].matched) {
                std::string ms_str = match[3];
                ms = std::stoi(ms_str);
                // Normalize: 1 digit = 100ms, 2 digits = 10ms, 3 digits = as-is
                switch (ms_str.length()) {
                    case 1: ms *= 100; break;
                    case 2: ms *= 10;  break;
                    case 3: /* as-is */ break;
                }
            }

            double timestamp = static_cast<double>(min * 60 + sec) + static_cast<double>(ms) / 1000.0;
            std::string text = match[4];

            // Trim whitespace
            text.erase(0, text.find_first_not_of(" \t\r\n"));
            text.erase(text.find_last_not_of(" \t\r\n") + 1);

            result.push_back({timestamp, text});
            it = match.suffix().first;
        }
    }

    // Sort by timestamp
    std::sort(result.begin(), result.end(),
              [](const LyricLine& a, const LyricLine& b) {
                  return a.time < b.time;
              });

    LOG_DEBUG("Parsed {} LRC lines", result.size());
    return result;
}

} // namespace lyrics
