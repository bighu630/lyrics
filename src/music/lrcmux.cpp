#include "music/lrcmux.h"
#include "util/log.h"

#include <chrono>

namespace lyrics {

// ── Constructor ──────────────────────────────────────────────────

LrcmuxClient::LrcmuxClient() {
    http_.set_base_url("https://api.lrcmux.dev");
    http_.set_timeout(std::chrono::seconds(5));
    http_.set_user_agent("lyrics-cpp/1.0");
}

// ── name ─────────────────────────────────────────────────────────

std::string LrcmuxClient::name() const {
    return "Lrcmux";
}

// ── get_lyrics ───────────────────────────────────────────────────

std::string LrcmuxClient::get_lyrics(const std::string& title,
                                      const std::string& artist,
                                      double /*duration*/) {
    LOG_DEBUG("Lrcmux: looking up title='{}' artist='{}'", title, artist);

    auto resp = http_.get("/get", {
        {"artist", artist},
        {"title", title},
        {"format", "lrc"}
    });

    if (!resp.ok()) {
        LOG_WARN("Lrcmux request failed: status={} error={}",
                 resp.status_code, resp.error);
        return {};
    }

    if (resp.body.empty() || resp.body.find_first_not_of(" \t\r\n") == std::string::npos) {
        LOG_WARN("Lrcmux returned empty/whitespace body for title='{}' artist='{}'", title, artist);
        return {};
    }

    LOG_INFO("Lrcmux: found lyrics for title='{}' artist='{}' ({} bytes)",
             title, artist, resp.body.size());
    return resp.body;
}

} // namespace lyrics
