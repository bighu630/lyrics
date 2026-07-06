#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "music/lrclib.h"
#include "music/netease.h"

namespace lyrics {

/// Multi-provider music manager.
/// Tries each provider in order and returns the first successful result.
class MusicManager {
public:
    /// Provider type enum
    enum class ProviderType {
        LRCLib,
        Netease
    };
    
    MusicManager();
    
    /// Add a provider to the chain.
    void add_provider(ProviderType type, const std::string& cookie = "");
    
    /// Get lyrics by song info. Tries all providers in order.
    /// @param title      Song title
    /// @param artist     Artist name
    /// @param duration   Song duration in seconds (0 if unknown)
    /// @return LRC lyrics text, or empty string if all providers fail
    std::string get_lyrics(const std::string& title, const std::string& artist, double duration);
    
    /// Number of registered providers.
    int provider_count() const { return (int)providers_.size(); }
    
private:
    struct Provider {
        std::string name;
        std::function<std::string(const std::string&, const std::string&, double)> get_lyrics_fn;
        Provider(std::string n,
                 std::function<std::string(const std::string&, const std::string&, double)> fn)
            : name(std::move(n)), get_lyrics_fn(std::move(fn)) {}
    };
    
    std::vector<Provider> providers_;
    
    // Provider instances
    std::unique_ptr<LRCLibClient> lrclib_;
    std::unique_ptr<NeteaseClient> netease_;
};

} // namespace lyrics
