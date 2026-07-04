#pragma once

#include <curl/curl.h>
#include <functional>
#include <memory>
#include <string>
#include <map>
#include <optional>
#include <chrono>

namespace lyrics {

/// RAII wrapper around libcurl's easy interface.
/// Provides synchronous HTTP GET and POST with timeout and retry.
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Non-copyable, movable
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    struct Response {
        long status_code = 0;
        std::string body;
        std::string error;
        bool ok() const { return status_code >= 200 && status_code < 300; }
    };

    /// Set default timeout (seconds).
    void set_timeout(std::chrono::seconds t) { timeout_ = t; }
    /// Set max retries on failure.
    void set_max_retries(int n) { max_retries_ = n; }
    /// Set base URL prefix (optional).
    void set_base_url(const std::string& url) { base_url_ = url; }
    /// Set User-Agent header.
    void set_user_agent(const std::string& ua) { user_agent_ = ua; }
    /// Set a default header.
    void set_header(const std::string& key, const std::string& value);
    /// Remove a default header.
    void remove_header(const std::string& key);
    /// Set cookie string.
    void set_cookie(const std::string& cookie) { cookie_ = cookie; }

    /// Perform a GET request.
    Response get(const std::string& path_or_url,
                 const std::map<std::string, std::string>& params = {});

    /// Perform a POST request with raw body.
    Response post(const std::string& path_or_url,
                  const std::string& content_type,
                  const std::string& body);

    /// Perform a POST with JSON body (sets Content-Type: application/json).
    Response post_json(const std::string& path_or_url,
                       const std::string& json_body);

private:
    struct CURLDeleter {
        void operator()(CURL* c) const { curl_easy_cleanup(c); }
    };
    using CURLPtr = std::unique_ptr<CURL, CURLDeleter>;

    CURLPtr handle_;
    std::string base_url_;
    std::string user_agent_;
    std::string cookie_;
    std::map<std::string, std::string> headers_;
    std::chrono::seconds timeout_{10};
    int max_retries_{3};

    /// Build full URL from path.
    std::string build_url(const std::string& path_or_url,
                          const std::map<std::string, std::string>& params) const;

    /// Execute one attempt.
    Response do_attempt(const std::string& url,
                        const std::string& post_data,
                        const std::string& content_type);

    /// Write callback for curl.
    static size_t write_cb(char* data, size_t size, size_t nmemb, void* userp);
    static size_t header_cb(char* data, size_t size, size_t nmemb, void* userp);
};

} // namespace lyrics
