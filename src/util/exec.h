#pragma once

#include <array>
#include <cstdio>
#include <memory>
#include <string>

namespace lyrics {
namespace util {

/// Execute a shell command and read stdout.
/// Returns trimmed output (trailing newlines removed).
/// Returns empty string on failure.
inline std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    // Use a lambda as deleter to avoid -Wignored-attributes with decltype(&pclose)
    auto deleter = [](FILE* f) { if (f) pclose(f); };
    std::unique_ptr<FILE, decltype(deleter)> pipe(
        popen(cmd, "r"),
        deleter);
    if (!pipe) {
        return {};
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Trim trailing newline/whitespace
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

} // namespace util
} // namespace lyrics
