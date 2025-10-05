#pragma once
#include <string>
#include <map>
#include <chrono>

class Config;
class HTTPClient;
class Logger;

class Auth {
public:
    struct Tokens {
        std::string access_token;   // SmartAPI "jwtToken"
        std::string refresh_token;  // SmartAPI "refreshToken"
        std::string feed_token;     // SmartAPI "feedToken" (for WS market data)
        std::chrono::system_clock::time_point expires_at{}; // best-effort if ttl known
    };

    // Borrow existing instances; no ownership.
    Auth(const Config& cfg, HTTPClient& http, Logger& log);

    // Login (password + TOTP). SmartAPI commonly uses TOTP; COTP can call this too.
    bool login_with_totp(const std::string& totp);

    // If you use COTP (SMS/email OTP) via same field, call this for symmetry.
    bool login_with_cotp(const std::string& cotp) { return login_impl(cotp); }

    // Refresh tokens (uses stored refresh_token).
    bool refresh();

    // Helpers
    bool is_expired(std::chrono::seconds skew = std::chrono::seconds(60)) const;
    const Tokens& tokens() const noexcept { return tokens_; }
    std::map<std::string,std::string> auth_headers() const; // {"Authorization":"Bearer <jwt>"}

private:
    const Config& cfg_;
    HTTPClient& http_;
    Logger& log_;
    Tokens tokens_;

    bool login_impl(const std::string& otp);
    bool handle_login_response(const std::string& body);
    bool handle_refresh_response(const std::string& body);
};

