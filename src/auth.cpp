#include "auth.h"
#include "config.h"
#include "http_client.h"
#include "logger.h"

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <cstdlib>

using json = nlohmann::json;

namespace {
// SmartAPI endpoints (Angel One)
constexpr const char* kBase      = "https://apiconnect.angelone.in";
constexpr const char* kLoginPath = "/rest/auth/angelbroking/user/v1/loginByPassword";
constexpr const char* kGenTok    = "/rest/auth/angelbroking/jwt/v1/generateTokens";

// Common headers. Some deployments expect X-PrivateKey etc.
// We keep it minimal and include api_key as X-PrivateKey; adjust if your account needs different.
std::map<std::string,std::string> common_headers(const Config& cfg) {
    return {
        {"Content-Type", "application/json"},
        {"Accept",       "application/json"},
        {"X-PrivateKey", cfg.api_key()},
        {"X-UserType",   "USER"},
        {"X-SourceID",   "WEB"}
        // You can add: X-ClientLocalIP / X-ClientPublicIP / X-MACAddress if needed.
    };
}
} // namespace

Auth::Auth(const Config& cfg, HTTPClient& http, Logger& log)
    : cfg_(cfg), http_(http), log_(log) {}

bool Auth::login_with_totp(const std::string& totp) {
    return login_impl(totp);
}

bool Auth::login_impl(const std::string& otp) {
    // Build URL and payload
    const std::string url = std::string(kBase) + kLoginPath;

    json payload = {
        {"clientcode", cfg_.client_code()},
        {"password",   cfg_.client_secret()}, // If you store password separately, adjust here.
        {"totp",       otp}
    };

    // POST
    auto hdrs = common_headers(cfg_);
    auto resp = http_.post_json(url, payload.dump(), hdrs);

    if (resp.status / 100 != 2) {
        log_.error("Auth.login failed: HTTP " + std::to_string(resp.status) + " body=" + resp.body);
        return false;
    }
    if (!handle_login_response(resp.body)) {
        log_.error("Auth.login failed: unable to parse/validate login response");
        return false;
    }
    log_.info("Auth.login success");
    return true;
}

bool Auth::refresh() {
    if (tokens_.refresh_token.empty()) {
        log_.warn("Auth.refresh called without refresh_token");
        return false;
    }

    const std::string url = std::string(kBase) + kGenTok;

    json payload = {
        {"refreshToken", tokens_.refresh_token}
        // Some variants also accept {"jwtToken": tokens_.access_token}, but refreshToken is enough.
    };

    auto hdrs = common_headers(cfg_);
    auto resp = http_.post_json(url, payload.dump(), hdrs);

    if (resp.status / 100 != 2) {
        log_.error("Auth.refresh failed: HTTP " + std::to_string(resp.status) + " body=" + resp.body);
        return false;
    }
    if (!handle_refresh_response(resp.body)) {
        log_.error("Auth.refresh failed: unable to parse/validate response");
        return false;
    }
    log_.info("Auth.refresh success");
    return true;
}

bool Auth::is_expired(std::chrono::seconds skew) const {
    if (tokens_.access_token.empty()) return true;
    if (tokens_.expires_at.time_since_epoch().count() == 0) {
        // No TTL known → treat as non-expiring (or force refresh by returning true)
        return false; // change to true if you prefer aggressive refreshing
    }
    return (std::chrono::system_clock::now() + skew) >= tokens_.expires_at;
}

std::map<std::string,std::string> Auth::auth_headers() const {
    if (tokens_.access_token.empty()) return {};
    return { {"Authorization", std::string("Bearer ") + tokens_.access_token} };
}

bool Auth::handle_login_response(const std::string& body) {
    // Expected shape (typical):
    // { "status": true, "data": { "jwtToken": "...", "refreshToken":"...", "feedToken":"..." }, ... }
    json j;
    try { j = json::parse(body); } catch (...) { return false; }

    if (!j.contains("data") || !j["data"].is_object()) return false;
    const auto& d = j["data"];

    if (!d.contains("jwtToken") || !d.contains("refreshToken")) return false;

    tokens_.access_token  = d.value("jwtToken", "");
    tokens_.refresh_token = d.value("refreshToken", "");
    tokens_.feed_token    = d.value("feedToken", "");

    // TTL handling: some responses provide "expiresIn" (seconds) or "jwtTokenTTL".
    // If present, compute expires_at; otherwise leave at epoch (unknown).
    int ttl_sec = 0;
    if (d.contains("expiresIn") && d["expiresIn"].is_number_integer())
        ttl_sec = d["expiresIn"].get<int>();
    else if (d.contains("jwtTokenTTL") && d["jwtTokenTTL"].is_number_integer())
        ttl_sec = d["jwtTokenTTL"].get<int>();

    if (ttl_sec > 0) {
        tokens_.expires_at = std::chrono::system_clock::now() + std::chrono::seconds(ttl_sec);
    } else {
        tokens_.expires_at = std::chrono::system_clock::time_point{}; // unknown
    }
    return !tokens_.access_token.empty();
}

bool Auth::handle_refresh_response(const std::string& body) {
    // Expected: { "status": true, "data": { "jwtToken": "...", "feedToken": "...", "refreshToken": "..."? } }
    json j;
    try { j = json::parse(body); } catch (...) { return false; }

    if (!j.contains("data") || !j["data"].is_object()) return false;
    const auto& d = j["data"];

    // jwtToken + (optional) new refreshToken
    const std::string new_jwt = d.value("jwtToken", "");
    if (new_jwt.empty()) return false;

    tokens_.access_token = new_jwt;
    if (d.contains("refreshToken")) tokens_.refresh_token = d.value("refreshToken", tokens_.refresh_token);
    tokens_.feed_token = d.value("feedToken", tokens_.feed_token);

    int ttl_sec = 0;
    if (d.contains("expiresIn") && d["expiresIn"].is_number_integer())
        ttl_sec = d["expiresIn"].get<int>();
    else if (d.contains("jwtTokenTTL") && d["jwtTokenTTL"].is_number_integer())
        ttl_sec = d["jwtTokenTTL"].get<int>();

    if (ttl_sec > 0) {
        tokens_.expires_at = std::chrono::system_clock::now() + std::chrono::seconds(ttl_sec);
    }
    return true;
}

