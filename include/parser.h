// include/parser.h
#pragma once
#include <string>
#include <optional>
#include <chrono>

struct LTP {
    std::string token;                                   // e.g. "nse_cm|26000" or raw "26000"
    double       ltp = 0.0;                              // last traded price
    std::chrono::system_clock::time_point ts{};          // event/server time if present
};

class Parser {
public:
    // Parse a single WS JSON frame into LTP. Returns nullopt if required fields missing/invalid.
    // Accepts common SmartAPI shapes, e.g.:
    //  { "symbol": "...", "ltp": 123.45, "exchange_timestamp": 1728123456789 }
    //  { "token": "...",  "last_price": 123.45, "timestamp": 1728123456 }
    std::optional<LTP> parse_ltp(const std::string& json_text) const;

    // Optional: normalize tokens by stripping known prefixes like "nse_cm|"
    void set_strip_prefix(const std::string& prefix);     // "" disables
    const std::string& strip_prefix() const noexcept;

private:
    std::string strip_prefix_{};

    // helpers (not exposed)
    static std::chrono::system_clock::time_point to_timepoint(long long ts_sec_or_ms);
};

