#pragma once
#include "parser.h"
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <string>

class LTPStore {
public:
    void upsert(const LTP& v);                          // token -> overwrite {ltp, ts}
    std::optional<LTP> get(const std::string& token) const;
    std::unordered_map<std::string, LTP> snapshot() const;
    std::size_t size() const;

private:
    mutable std::shared_mutex mu_;
    std::unordered_map<std::string, LTP> map_;
};

