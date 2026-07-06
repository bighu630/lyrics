#include "gemini.h"

#include "util/http.h"
#include "util/json.h"
#include "util/log.h"

namespace lyrics {

GeminiClient::GeminiClient(std::string api_key, std::string model)
    : api_key_(std::move(api_key))
    , model_(std::move(model)) {
    if (model_.empty()) {
        model_ = "gemini-2.0-flash-exp";
    }
}

std::string GeminiClient::handle_text(const std::string& prompt) {
    // Build URL
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/"
                      + model_ + ":generateContent?key=" + api_key_;

    // Build JSON request body using JsonDoc
    JsonDoc body = JsonDoc::make_object();
    yyjson_mut_doc* doc = body.doc_mut();
    yyjson_mut_val* root = body.root_mut();

    // contents array
    yyjson_mut_val* contents = yyjson_mut_arr(doc);
    yyjson_mut_val* contents_key = yyjson_mut_strcpy(doc, "contents");
    yyjson_mut_obj_add(root, contents_key, contents);

    // content object
    yyjson_mut_val* content = yyjson_mut_obj(doc);
    yyjson_mut_arr_append(contents, content);

    // parts array
    yyjson_mut_val* parts = yyjson_mut_arr(doc);
    yyjson_mut_val* parts_key = yyjson_mut_strcpy(doc, "parts");
    yyjson_mut_obj_add(content, parts_key, parts);

    // part object with text
    yyjson_mut_val* part = yyjson_mut_obj(doc);
    yyjson_mut_arr_append(parts, part);
    JsonDoc::set_str(part, doc, "text", prompt);

    std::string json_body = body.to_string();

    LOG_DEBUG("Gemini request to model '{}': {}", model_, json_body);

    // Execute request
    HttpClient client;
    client.set_timeout(std::chrono::seconds(60));
    auto resp = client.post_json(url, json_body);

    if (!resp.ok()) {
        LOG_ERROR("Gemini API error [{}]: {}", resp.status_code, resp.body);
        return {};
    }

    LOG_DEBUG("Gemini response received ({} bytes)", resp.body.size());

    // Parse response
    JsonDoc response(resp.body);
    if (!response.valid()) {
        LOG_ERROR("Failed to parse Gemini response JSON: {}", resp.body);
        return {};
    }

    // Extract candidates[0].content.parts[0].text
    yyjson_val* text_val = JsonDoc::get_path(response.root(),
                                             "candidates[0].content.parts[0].text");
    if (text_val && yyjson_is_str(text_val)) {
        std::string result = yyjson_get_str(text_val);
        LOG_DEBUG("Gemini result: {}", result);
        return result;
    }

    // Try to extract error message if available
    yyjson_val* error_val = JsonDoc::get_path(response.root(), "error.message");
    if (error_val && yyjson_is_str(error_val)) {
        LOG_ERROR("Gemini API returned error: {}", yyjson_get_str(error_val));
    } else {
        LOG_ERROR("Gemini response missing expected path 'candidates[0].content.parts[0].text': {}",
                  resp.body);
    }

    return {};
}

} // namespace lyrics
