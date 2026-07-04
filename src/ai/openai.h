#pragma once

#include "ai_interface.h"

#include <string>

namespace lyrics {

/// OpenAI API client implementing AiInterface.
class OpenAIClient : public AiInterface {
public:
    /// Construct with API key, model name, and optional base URL.
    /// If model is empty, defaults to "gpt-3.5-turbo".
    /// If base_url is empty, defaults to "https://api.openai.com/v1".
    explicit OpenAIClient(std::string api_key,
                          std::string model_name,
                          std::string base_url = "");

    std::string name() const override { return "openai"; }
    std::string handle_text(const std::string& prompt) override;

private:
    std::string api_key_;
    std::string model_;
    std::string base_url_;
};

} // namespace lyrics
