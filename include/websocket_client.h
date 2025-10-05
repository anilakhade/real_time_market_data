// include/websocket_client.h
#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <chrono>
#include <map>

class Logger;

class WebSocketClient {
public:
    using MessageCallback = std::function<void(const std::string& /*msg*/)>;   // raw frames (text/binary)
    using StateCallback   = std::function<void(const std::string& /*state*/)>; // "connecting","connected","closed","reconnecting","failed"
    using ResubscribeFn   = std::function<void(WebSocketClient&)>;             // called right after reconnect

    struct Options {
        std::chrono::seconds ping_interval{15};             // periodic ping
        std::chrono::seconds conn_timeout{10};              // TCP/TLS connect timeout
        bool verify_peer{true};                             // TLS verify
        std::string ca_file;                                 // optional CA bundle path
        std::map<std::string,std::string> headers;           // extra handshake headers
        // reconnect backoff
        std::chrono::milliseconds backoff_initial{500};
        std::chrono::milliseconds backoff_max{5000};
    };

    WebSocketClient(std::string wss_url, Logger& log);
    WebSocketClient(std::string wss_url, Logger& log, Options opts);
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    // Lifecycle
    bool start();     // spawn IO thread, connect, begin read loop
    void stop();      // graceful stop + join

    // I/O
    bool send_text(const std::string& payload);
    bool send_binary(const void* data, size_t len);

    // Callbacks (set anytime; invoked from IO thread)
    void on_message(MessageCallback cb);
    void on_state(StateCallback cb);
    void on_resubscribe(ResubscribeFn fn);

    // Introspection
    bool is_connected() const noexcept { return connected_.load(); }
    const std::string& url() const noexcept { return url_; }

private:
    struct Impl;        // pimpl to keep Boost.Beast/Asio out of headers
    Impl* impl_;

    std::string url_;
    Logger& log_;
    Options opts_;
    std::atomic<bool> connected_{false};
};

