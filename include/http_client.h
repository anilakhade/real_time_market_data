#pragma once
#include <string>
#include <map>
#include <chrono>

struct HttpResponse {
    int status = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

class HTTPClient {
public:
    struct Options {
        std::chrono::milliseconds timeout{std::chrono::seconds(10)};
        bool verify_peer = true;                         // TLS cert verification
        std::string ca_file;                             // optional CA bundle path
        std::string user_agent = "alpha-http/1.0";
        std::map<std::string, std::string> default_headers;
    };

    HTTPClient();
    explicit HTTPClient(Options opts);

    // Simple HTTPS GET: https_url must start with https://
    HttpResponse get(const std::string& https_url,
                     const std::map<std::string, std::string>& headers = {},
                     const std::map<std::string, std::string>& query = {});

    // Generic POST with explicit content-type
    HttpResponse post(const std::string& https_url,
                      const std::string& body,
                      const std::string& content_type = "application/octet-stream",
                      const std::map<std::string, std::string>& headers = {},
                      const std::map<std::string, std::string>& query = {});

    // Convenience: JSON POST (sets Content-Type: application/json)
    HttpResponse post_json(const std::string& https_url,
                           const std::string& json_body,
                           const std::map<std::string, std::string>& headers = {},
                           const std::map<std::string, std::string>& query = {});

    // Manage default headers
    void set_default_header(const std::string& key, const std::string& value);
    void erase_default_header(const std::string& key);

    // Access options
    const Options& options() const noexcept { return opts_; }

private:
    struct UrlParts {
        std::string host;
        std::string port;   // "443" by default
        std::string target; // path + '?' + query
    };

    static UrlParts parse_https_url(const std::string& https_url,
                                    const std::map<std::string, std::string>& query);

    static std::string build_query_string(const std::map<std::string, std::string>& query);

    Options opts_;
    // Non-copyable, movable (socket state lives in impl .cpp)
    HTTPClient(const HTTPClient&) = delete;
    HTTPClient& operator=(const HTTPClient&) = delete;
    HTTPClient(HTTPClient&&) noexcept = default;
    HTTPClient& operator=(HTTPClient&&) noexcept = default;
};
