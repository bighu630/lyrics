#include "util/http.h"
#include "util/log.h"

namespace lyrics {

// ── Constructor / Destructor ─────────────────────────────────────

HttpClient::HttpClient()
    : handle_(curl_easy_init()) {
    if (!handle_) {
        throw std::runtime_error("failed to initialize libcurl");
    }
    user_agent_ = "lyrics-cpp/1.0";
}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&&) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

// ── Header management ────────────────────────────────────────────

void HttpClient::set_header(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpClient::remove_header(const std::string& key) {
    headers_.erase(key);
}

// ── URL building ─────────────────────────────────────────────────

std::string HttpClient::build_url(
    const std::string& path_or_url,
    const std::map<std::string, std::string>& params) const {

    // If it already looks like a full URL, use as-is
    std::string url;
    if (path_or_url.find("://") != std::string::npos) {
        url = path_or_url;
    } else {
        url = base_url_ + path_or_url;
    }

    // Append query parameters
    if (!params.empty()) {
        url += "?";
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (it != params.begin()) url += "&";
            char* enc_key = curl_easy_escape(handle_.get(), it->first.c_str(), (int)it->first.size());
            char* enc_val = curl_easy_escape(handle_.get(), it->second.c_str(), (int)it->second.size());
            url += enc_key;
            url += "=";
            url += enc_val;
            curl_free(enc_key);
            curl_free(enc_val);
        }
    }
    return url;
}

// ── Single attempt ───────────────────────────────────────────────

HttpClient::Response HttpClient::do_attempt(
    const std::string& url,
    const std::string& post_data,
    const std::string& content_type) {

    Response resp;
    curl_easy_reset(handle_.get());

    curl_easy_setopt(handle_.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle_.get(), CURLOPT_TIMEOUT_MS, (long)(timeout_.count() * 1000));
    curl_easy_setopt(handle_.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle_.get(), CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(handle_.get(), CURLOPT_USERAGENT, user_agent_.c_str());

    // Response body
    std::string body;
    curl_easy_setopt(handle_.get(), CURLOPT_WRITEFUNCTION, &HttpClient::write_cb);
    curl_easy_setopt(handle_.get(), CURLOPT_WRITEDATA, &body);

    // POST data
    if (!post_data.empty()) {
        curl_easy_setopt(handle_.get(), CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(handle_.get(), CURLOPT_POSTFIELDSIZE, (long)post_data.size());
    }

    // Build header list
    struct curl_slist* hlist = nullptr;
    if (!content_type.empty()) {
        std::string ct = "Content-Type: " + content_type;
        hlist = curl_slist_append(hlist, ct.c_str());
    }
    for (const auto& [k, v] : headers_) {
        std::string h = k + ": " + v;
        hlist = curl_slist_append(hlist, h.c_str());
    }
    if (!cookie_.empty()) {
        std::string c = "Cookie: " + cookie_;
        hlist = curl_slist_append(hlist, c.c_str());
    }
    if (hlist) {
        curl_easy_setopt(handle_.get(), CURLOPT_HTTPHEADER, hlist);
    }

    CURLcode res = curl_easy_perform(handle_.get());
    if (hlist) curl_slist_free_all(hlist);

    if (res != CURLE_OK) {
        resp.error = curl_easy_strerror(res);
        LOG_ERROR("HTTP request failed: {} (url={})", resp.error, url);
        return resp;
    }

    curl_easy_getinfo(handle_.get(), CURLINFO_RESPONSE_CODE, &resp.status_code);
    resp.body = std::move(body);
    return resp;
}

// ── Public methods ───────────────────────────────────────────────

HttpClient::Response HttpClient::get(
    const std::string& path_or_url,
    const std::map<std::string, std::string>& params) {

    std::string url = build_url(path_or_url, params);

    for (int attempt = 0; attempt <= max_retries_; ++attempt) {
        if (attempt > 0) {
            LOG_WARN("Retrying GET request (attempt {}/{})", attempt, max_retries_);
            std::this_thread::sleep_for(std::chrono::milliseconds(500 * attempt));
        }
        auto resp = do_attempt(url, "", "");
        if (resp.ok()) return resp;
        if (attempt == max_retries_) return resp;
    }
    return {}; // unreachable
}

HttpClient::Response HttpClient::post(
    const std::string& path_or_url,
    const std::string& content_type,
    const std::string& body) {

    std::string url = build_url(path_or_url, {});

    for (int attempt = 0; attempt <= max_retries_; ++attempt) {
        if (attempt > 0) {
            LOG_WARN("Retrying POST request (attempt {}/{})", attempt, max_retries_);
            std::this_thread::sleep_for(std::chrono::milliseconds(500 * attempt));
        }
        auto resp = do_attempt(url, body, content_type);
        if (resp.ok()) return resp;
        if (attempt == max_retries_) return resp;
    }
    return {};
}

HttpClient::Response HttpClient::post_json(
    const std::string& path_or_url,
    const std::string& json_body) {
    return post(path_or_url, "application/json", json_body);
}

// ── Callbacks ────────────────────────────────────────────────────

size_t HttpClient::write_cb(char* data, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* buf = static_cast<std::string*>(userp);
    buf->append(data, total);
    return total;
}

size_t HttpClient::header_cb(char* data, size_t size, size_t nmemb, void* userp) {
    (void)data;
    (void)userp;
    return size * nmemb;
}

} // namespace lyrics
