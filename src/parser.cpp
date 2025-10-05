// src/parser.cpp
#include "parser.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <string>
#include <vector>

using json = nlohmann::json;

// ---- helpers ---------------------------------------------------------------

static bool get_string_any(const json& j, const std::vector<const char*>& keys, std::string& out) {
    for (auto k : keys) {
        if (j.contains(k)) {
            const auto& v = j.at(k);
            if (v.is_string()) { out = v.get<std::string>(); return true; }
            if (v.is_number_integer() || v.is_number_float()) { out = v.dump(); return true; }
        }
    }
    return false;
}

static bool get_number_any(const json& j, const std::vector<const char*>& keys, double& out) {
    for (auto k : keys) {
        if (j.contains(k)) {
            const auto& v = j.at(k);
            if (v.is_number_float())     { out = v.get<double>(); return true; }
            if (v.is_number_integer())   { out = static_cast<double>(v.get<long long>()); return true; }
            if (v.is_string()) { // tolerate numeric-as-string
                try { out = std::stod(v.get<std::string>()); return true; } catch (...) {}
            }
        }
    }
    return false;
}

static bool get_time_any(const json& j, const std::vector<const char*>& keys, long long& out) {
    for (auto k : keys) {
        if (j.contains(k)) {
            const auto& v = j.at(k);
            if (v.is_number_integer())   { out = v.get<long long>(); return true; }
            if (v.is_number_float())     { out = static_cast<long long>(v.get<double>()); return true; }
            if (v.is_string()) {
                try { out = std::stoll(v.get<std::string>()); return true; } catch (...) {}
            }
        }
    }
    return false;
}

// ---- Parser ----------------------------------------------------------------

std::chrono::system_clock::time_point
Parser::to_timepoint(long long ts_sec_or_ms) {
    // Heuristic: >= 10^12 → milliseconds since epoch; else seconds.
    if (std::llabs(ts_sec_or_ms) >= 1000000000000LL) {
        return std::chrono::system_clock::time_point(std::chrono::milliseconds(ts_sec_or_ms));
    }
    return std::chrono::system_clock::time_point(std::chrono::seconds(ts_sec_or_ms));
}

void Parser::set_strip_prefix(const std::string& p) { strip_prefix_ = p; }
const std::string& Parser::strip_prefix() const noexcept { return strip_prefix_; }

std::optional<LTP> Parser::parse_ltp(const std::string& json_text) const {
    json j;
    try { j = json::parse(json_text); }
    catch (...) { return std::nullopt; }

    // Some feeds wrap payload under "data" or an array; unwrap if needed.
    if (j.is_array() && !j.empty()) j = j.front();
    if (j.contains("data")) {
        const auto& d = j["data"];
        if (d.is_object()) j = d;
        else if (d.is_array() && !d.empty()) j = d.front();
    }

    // Token keys commonly seen across brokers/feeds
    static const std::vector<const char*> TOKEN_KEYS{
        "token", "symbol", "tradingsymbol", "instrument_token", "tokenID"
    };
    // Price keys
    static const std::vector<const char*> PRICE_KEYS{
        "ltp", "last_price", "lastPrice", "price", "trade_price"
    };
    // Timestamp keys (seconds or milliseconds)
    static const std::vector<const char*> TS_KEYS{
        "exchange_timestamp", "timestamp", "ts", "time", "epoch"
    };

    std::string token;
    double price = 0.0;
    if (!get_string_any(j, TOKEN_KEYS, token)) return std::nullopt;
    if (!get_number_any(j, PRICE_KEYS, price)) return std::nullopt;

    // Optional ts
    long long ts_raw = 0;
    std::chrono::system_clock::time_point tp{};
    if (get_time_any(j, TS_KEYS, ts_raw)) {
        tp = to_timepoint(ts_raw);
    }

    // Normalize token (optional prefix strip)
    if (!strip_prefix_.empty() && token.rfind(strip_prefix_, 0) == 0) {
        token.erase(0, strip_prefix_.size());
    }

    LTP out;
    out.token = std::move(token);
    out.ltp   = price;
    out.ts    = tp;
    return out;
}

