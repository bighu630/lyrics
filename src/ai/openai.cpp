#include "openai.h"

#include "util/http.h"
#include "util/json.h"
#include "util/log.h"

namespace lyrics {

OpenAIClient::OpenAIClient(std::string api_key,
                           std::string model_name,
                           std::string base_url)
    : api_key_(std::move(api_key))
    , model_(std::move(model_name))
    , base_url_(std::move(base_url)) {
    if (model_.empty()) {
        model_ = "gpt-3.5-turbo";
    }
    if (base_url_.empty()) {
        base_url_ = "https://api.openai.com/v1";
    }
    // Ensure no trailing slash
    if (base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

std::string OpenAIClient::handle_text(const std::string& prompt) {
    std::string url = base_url_ + "/chat/completions";

    // Build JSON request body
    JsonDoc body = JsonDoc::make_object();
    yyjson_mut_doc* doc = body.doc_mut();
    yyjson_mut_val* root = body.root_mut();

    JsonDoc::set_str(root, doc, "model", model_);

    double temperature_val = 0.3;
    yyjson_mut_obj_add_real(doc, root, "temperature", temperature_val);

    // messages array
    yyjson_mut_val* messages = yyjson_mut_arr(doc);
    yyjson_mut_val* msg_key = yyjson_mut_strcpy(doc, "messages");
    yyjson_mut_obj_add(root, msg_key, messages);

    // message object
    yyjson_mut_val* message = yyjson_mut_obj(doc);
    yyjson_mut_arr_append(messages, message);
    JsonDoc::set_str(message, doc, "role", "user");
    JsonDoc::set_str(message, doc, "content", prompt);

    // max_tokens
    JsonDoc::set_int(root, doc, "max_tokens", 2000);

    std::string json_body = body.to_string();

    LOG_DEBUG("OpenAI request to model '{}': {}", model_, json_body);

    // Execute request with auth header
    HttpClient client;
    client.set_timeout(std::chrono::seconds(60));
    client.set_header("Authorization", "Bearer " + api_key_);
    auto resp = client.post_json(url, json_body);

    if (!resp.ok()) {
        LOG_ERROR("OpenAI API error [{}]: {}", resp.status_code, resp.body);
        return {};
    }

    LOG_DEBUG("OpenAI response received ({} bytes)", resp.body.size());

    // Parse response
    JsonDoc response(resp.body);
    if (!response.valid()) {
        LOG_ERROR("Failed to parse OpenAI response JSON: {}", resp.body);
        return {};
    }

    // Extract choices[0].message.content
    yyjson_val* content_val = JsonDoc::get_path(response.root(),
                                                "choices[0].message.content");
    if (content_val && yyjson_is_str(content_val)) {
        std::string result = yyjson_get_str(content_val);
        LOG_DEBUG("OpenAI result: {}", result);
        return result;
    }

    // Try to extract error details
    yyjson_val* error_val = JsonDoc::get_path(response.root(), "error.message");
    if (error_val && yyjson_is_str(error_val)) {
        LOG_ERROR("OpenAI API returned error: {}", yyjson_get_str(error_val));
    } else {
        LOG_ERROR("OpenAI response missing expected path 'choices[0].message.content': {}",
                  resp.body);
    }

    return {};
}

} // namespace lyrics
