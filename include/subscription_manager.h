// include/subscription_manager.h
#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <mutex>

class Logger;

class SubscriptionManager {
public:
    enum class Mode { LTP, QUOTE, FULL };

    // token_formatter: optional transform (e.g. "nse_cm|"+token)
    explicit SubscriptionManager(Logger& log,
                                 Mode mode = Mode::LTP,
                                 std::size_t batch_size = 100,
                                 std::function<std::string(const std::string&)> token_formatter = nullptr);

    // Desired set mutations
    void add(const std::string& token);
    void add_many(const std::vector<std::string>& tokens);
    void remove(const std::string& token);
    void clear();

    // Configuration
    void set_mode(Mode m);
    void set_batch_size(std::size_t n);
    void set_token_formatter(std::function<std::string(const std::string&)> fmt);

    // Build payloads to move server state toward desired:
    // - subscribe batches for tokens that are in desired but not active
    // - unsubscribe batches for tokens that are active but not desired
    std::vector<std::string> build_subscribe_batches() const;
    std::vector<std::string> build_unsubscribe_batches() const;

    // Call when the server ACKs to keep 'active' in sync
    void mark_subscribed(const std::vector<std::string>& tokens);
    void mark_unsubscribed(const std::vector<std::string>& tokens);

    // Snapshots (for metrics/logs)
    std::vector<std::string> desired_snapshot() const;
    std::vector<std::string> active_snapshot() const;

private:
    // Internal helpers
    std::vector<std::string> diff_desired_minus_active() const;   // needs subscribe
    std::vector<std::string> diff_active_minus_desired() const;   // needs unsubscribe
    std::string build_payload(const std::vector<std::string>& batch, bool subscribe) const;

    std::string mode_string() const; // "ltp"/"quote"/"full"
    std::vector<std::vector<std::string>> make_batches(const std::vector<std::string>& items) const;
    std::string fmt_token(const std::string& t) const;

    Logger& log_;
    Mode mode_;
    std::size_t batch_size_;
    std::function<std::string(const std::string&)> token_formatter_; // optional

    mutable std::mutex mu_;
    std::unordered_set<std::string> desired_;
    std::unordered_set<std::string> active_;
};

