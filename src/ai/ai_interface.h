#pragma once

#include <string>
#include <regex>

namespace lyrics {

/// Interface for AI text processing (song identification).
class AiInterface {
public:
    virtual ~AiInterface() = default;
    virtual std::string name() const = 0;
    virtual std::string handle_text(const std::string& prompt) = 0;

    /// Build a prompt for song identification from a title.
    /// The prompt instructs the AI to return a specific JSON format.
    static std::string format_query_song(const std::string& title) {
        return "Please accurately extract song information in the following JSON format:\n"
               "Input is a media title string. If it contains song info, return:\n"
               "{\"is_song\": true, \"title\": \"Song Title\", \"artist\": \"Artist Name\"}.\n"
               "If it's not a song, return {\"is_song\": false}.\n"
               "Example: \"\u5c71\u5439\u83cc - \u300e\u7edd\u7f8e\u620f\u8154\u300f\u5c11\u5e74\u971c/\u63d0\u7cef-\u975e\u674e\" "
               "-> {is_song: true, title:\"\u975e\u674e\", artist:\"\u5c11\u5e74\u971c\"}.\n\n"
               "Note: \"title\" and \"artist\" must be accurate, otherwise it's an error.\n"
               "Note: Traditional Chinese in title must be converted to Simplified Chinese.\n\n"
               "Input media title: " + title;
    }

    /// Extract the first JSON object containing {\"is_song\" from an AI response.
    static std::string extract_song_json(const std::string& input) {
        // Flatten: remove newlines, extra spaces
        std::string flat;
        flat.reserve(input.size());
        for (char c : input) {
            if (c != '\n' && c != '\r' && c != '\t') flat += c;
        }
        // Match a JSON object containing "is_song"
        std::regex pattern(R"(\{[^{}]*"is_song"\s*:\s*(true|false)[^{}]*\})");
        std::smatch match;
        if (std::regex_search(flat, match, pattern)) {
            return match.str();
        }
        return "";
    }
};

} // namespace lyrics
