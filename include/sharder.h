// include/sharder.h
#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstddef>
#include <atomic>

class Logger;
class LTPStore;
class Parser;

class Sharder {
public:
    struct Options {
        std::string wss_url;                    // e.g. SmartAPI marketdata WSS URL
        std::size_t max_tokens_per_conn = 800;  // shard size (per WS)
        std::size_t subscribe_batch_size = 100; // per subscribe payload
        bool verify_peer = true;                // TLS verify
        std::string ca_file;                    // optional CA bundle
        std::string token_prefix = "nse_cm|";   // applied by SubscriptionManager
        // Extra HTTP headers for WS handshake (e.g., auth)
        std::map<std::string,std::string> headers;
    };

    // Dependencies injected:
    // - logger: shared app logger
    // - parser: shared Parser instance (used by all Consumers)
    // - store:  shared LTPStore (all Consumers upsert here)
    Sharder(Logger& log, Parser& parser, LTPStore& store, Options opts);

    ~Sharder();

    Sharder(const Sharder&) = delete;
    Sharder& operator=(const Sharder&) = delete;

    // Provide/refresh auth token (sets Authorization header or X-PrivateKey as needed)
    // Example: set_access_token("Bearer <JWT>");
    void set_access_token(const std::string& auth_header_value);

    // Replace or extend handshake headers (merged with access token header)
    void set_common_headers(const std::map<std::string,std::string>& hdrs);

    // Configure/replace the full desired token list (raw tokens, e.g., "26000")
    void set_tokens(const std::vector<std::string>& tokens);

    // Start all shards (build N workers, connect, subscribe)
    bool start();

    // Stop all shards and join threads
    void stop();

    // Introspection
    bool running() const noexcept;
    std::size_t num_workers() const noexcept;
    std::vector<std::string> desired_tokens_snapshot() const;

    bool debug_broadcast_text(const std::string& payload); // test-only helper

private:
    struct Worker;            // one WS stack (WS + SubMgr + Queue + Consumer)
    struct Impl;
    Impl* impl_;              // pimpl
};

