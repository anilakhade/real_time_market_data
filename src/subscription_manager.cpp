#include "subscription_manager.h"
#include "logger.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <unordered_set>

using json = nlohmann::json;

// ---- public ----
SubscriptionManager::SubscriptionManager(Logger& log,
                                         Mode mode,
                                         std::size_t batch_size,
                                         std::function<std::string(const std::string&)> token_formatter)
    : log_(log),
      mode_(mode),
      batch_size_(batch_size ? batch_size : 100),
      token_formatter_(std::move(token_formatter)) {}

void SubscriptionManager::add(const std::string& token) {
    std::lock_guard<std::mutex> lk(mu_);
    desired_.insert(token);
}

void SubscriptionManager::add_many(const std::vector<std::string>& tokens) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& t : tokens) desired_.insert(t);
}

void SubscriptionManager::remove(const std::string& token) {
    std::lock_guard<std::mutex> lk(mu_);
    desired_.erase(token);
}

void SubscriptionManager::clear() {
    std::lock_guard<std::mutex> lk(mu_);
    desired_.clear();
}

void SubscriptionManager::set_mode(Mode m) {
    std::lock_guard<std::mutex> lk(mu_);
    mode_ = m;
}

void SubscriptionManager::set_batch_size(std::size_t n) {
    std::lock_guard<std::mutex> lk(mu_);
    batch_size_ = (n ? n : 100);
}

void SubscriptionManager::set_token_formatter(std::function<std::string(const std::string&)> fmt) {
    std::lock_guard<std::mutex> lk(mu_);
    token_formatter_ = std::move(fmt);
}

std::vector<std::string> SubscriptionManager::build_subscribe_batches() const {
    std::vector<std::string> need;
    {
        std::lock_guard<std::mutex> lk(mu_);
        need = diff_desired_minus_active();
    }
    std::vector<std::string> out;
    for (auto& batch : make_batches(need)) {
        out.push_back(build_payload(batch, /*subscribe=*/true));
    }
    return out;
}

std::vector<std::string> SubscriptionManager::build_unsubscribe_batches() const {
    std::vector<std::string> need;
    {
        std::lock_guard<std::mutex> lk(mu_);
        need = diff_active_minus_desired();
    }
    std::vector<std::string> out;
    for (auto& batch : make_batches(need)) {
        out.push_back(build_payload(batch, /*subscribe=*/false));
    }
    return out;
}

void SubscriptionManager::mark_subscribed(const std::vector<std::string>& tokens) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& t : tokens) active_.insert(t);
}

void SubscriptionManager::mark_unsubscribed(const std::vector<std::string>& tokens) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& t : tokens) active_.erase(t);
}

std::vector<std::string> SubscriptionManager::desired_snapshot() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> v; v.reserve(desired_.size());
    for (auto& t : desired_) v.push_back(t);
    return v;
}

std::vector<std::string> SubscriptionManager::active_snapshot() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> v; v.reserve(active_.size());
    for (auto& t : active_) v.push_back(t);
    return v;
}

// ---- private helpers ----
std::vector<std::string> SubscriptionManager::diff_desired_minus_active() const {
    std::vector<std::string> res;
    res.reserve(desired_.size());
    for (auto& t : desired_) if (!active_.count(t)) res.push_back(t);
    return res;
}

std::vector<std::string> SubscriptionManager::diff_active_minus_desired() const {
    std::vector<std::string> res;
    res.reserve(active_.size());
    for (auto& t : active_) if (!desired_.count(t)) res.push_back(t);
    return res;
}

std::string SubscriptionManager::build_payload(const std::vector<std::string>& batch, bool subscribe) const {
    json j;
    j["action"] = subscribe ? "subscribe" : "unsubscribe";
    j["mode"]   = mode_string();

    std::vector<std::string> toks; toks.reserve(batch.size());
    for (auto& t : batch) toks.push_back(fmt_token(t));
    j["tokens"] = std::move(toks);

    return j.dump();
}

std::string SubscriptionManager::mode_string() const {
    switch (mode_) {
        case Mode::LTP:   return "ltp";
        case Mode::QUOTE: return "quote";
        case Mode::FULL:  return "full";
    }
    return "ltp";
}

std::vector<std::vector<std::string>>
SubscriptionManager::make_batches(const std::vector<std::string>& items) const {
    std::vector<std::vector<std::string>> batches;
    if (items.empty()) return batches;

    const std::size_t n = batch_size_ ? batch_size_ : 100;
    batches.reserve((items.size() + n - 1) / n);

    for (std::size_t i = 0; i < items.size(); i += n) {
        auto last = std::min(items.size(), i + n);
        batches.emplace_back(items.begin() + static_cast<std::ptrdiff_t>(i),
                             items.begin() + static_cast<std::ptrdiff_t>(last));
    }
    return batches;
}

std::string SubscriptionManager::fmt_token(const std::string& t) const {
    if (token_formatter_) return token_formatter_(t);
    return t;
}

