#pragma once

#include "ai_interface.h"

#include <string>

namespace lyrics {

/// Gemini API client implementing AiInterface.
class GeminiClient : public AiInterface {
public:
    /// Construct with API key and optional model name.
    explicit GeminiClient(std::string api_key,
                          std::string model = "gemini-2.0-flash-exp");

    std::string name() const override { return "gemini"; }
    std::string handle_text(const std::string& prompt) override;

private:
    std::string api_key_;
    std::string model_;
};

} // namespace lyrics
