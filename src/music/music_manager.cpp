#include "music/music_manager.h"
#include "util/log.h"

namespace lyrics {

MusicManager::MusicManager() {}

void MusicManager::add_provider(ProviderType type, const std::string& cookie) {
    switch (type) {
    case ProviderType::LRCLib: {
        LOG_DEBUG("MusicManager: adding LRCLib provider");
        auto client = std::make_unique<LRCLibClient>();
        providers_.push_back(Provider(
            "LRCLib",
            std::function<std::string(const std::string&, const std::string&, double)>(
                [this](const std::string& title, const std::string& artist, double duration) -> std::string {
                    return lrclib_->get_lyrics(title, artist, duration);
                }
            )
        ));
        lrclib_ = std::move(client);
        break;
    }
    case ProviderType::Lrcmux: {
        LOG_DEBUG("MusicManager: adding LRCLib provider");
        auto client = std::make_unique<LRCLibClient>();
        providers_.push_back(Provider(
            "LRCLib",
            std::function<std::string(const std::string&, const std::string&, double)>(
                [this](const std::string& title, const std::string& artist, double duration) -> std::string {
                    return lrclib_->get_lyrics(title, artist, duration);
                }
            )
        ));
        lrclib_ = std::move(client);
        break;
    }
    case ProviderType::Netease: {
        LOG_DEBUG("MusicManager: adding Netease provider");
        auto client = std::make_unique<NeteaseClient>(cookie);
        providers_.push_back(Provider(
            "Netease",
            std::function<std::string(const std::string&, const std::string&, double)>(
                [this](const std::string& title, const std::string& artist, double) -> std::string {
                    return netease_->get_lyrics(title, artist);
                }
            )
        ));
        netease_ = std::move(client);
        break;
    }
    }
}

std::string MusicManager::get_lyrics(const std::string& title,
                                     const std::string& artist,
                                     double duration) {
    for (auto& provider : providers_) {
        LOG_DEBUG("MusicManager: trying provider '{}' for '{}' - '{}'",
                  provider.name, title, artist);
        std::string result = provider.get_lyrics_fn(title, artist, duration);
        if (!result.empty()) {
            LOG_INFO("MusicManager: provider '{}' succeeded for '{}' - '{}'",
                     provider.name, title, artist);
            return result;
        }
        LOG_WARN("MusicManager: provider '{}' failed for '{}' - '{}'",
                 provider.name, title, artist);
    }
    LOG_WARN("MusicManager: all providers failed for '{}' - '{}'", title, artist);
    return "";
}

} // namespace lyrics
