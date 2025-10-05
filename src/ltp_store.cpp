#include "ltp_store.h"
#include <shared_mutex>   // for std::shared_mutex, std::shared_lock
#include <mutex>          // for std::unique_lock

void LTPStore::upsert(const LTP& v) {
    std::unique_lock<std::shared_mutex> lk(mu_);
    map_[v.token] = v;
}

std::optional<LTP> LTPStore::get(const std::string& token) const {
    std::shared_lock<std::shared_mutex> lk(mu_);
    auto it = map_.find(token);
    if (it == map_.end()) return std::nullopt;
    return it->second;
}

std::unordered_map<std::string, LTP> LTPStore::snapshot() const {
    std::shared_lock<std::shared_mutex> lk(mu_);
    return map_; // copy
}

std::size_t LTPStore::size() const {
    std::shared_lock<std::shared_mutex> lk(mu_);
    return map_.size();
}

