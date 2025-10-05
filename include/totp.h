#pragma once
#include <string>
#include <cstdint>
#include <chrono>

enum class TOTPAlgo { SHA1, SHA256, SHA512 };

class TOTP {
public:
    TOTP(std::string secret_base32,
         int digits = 6,
         std::chrono::seconds period = std::chrono::seconds(30),
         TOTPAlgo algo = TOTPAlgo::SHA1);

    std::string code_at(std::chrono::system_clock::time_point tp) const;

    std::string now() const;

    bool verify(const std::string& code,
                std::chrono::system_clock::time_point tp,
                int window_steps = 1) const;

private:
    std::string secret_; // raw bytes after Base32 decode
    int digits_;
    std::chrono::seconds period_;
    TOTPAlgo algo_;

    // helpers
    static std::string base32_decode(const std::string& b32); // returns raw bytes
    static uint64_t time_counter(std::chrono::system_clock::time_point tp, std::chrono::seconds period);
    static std::string hotp(const std::string& key, uint64_t counter,
                            int digits, TOTPAlgo algo); // HMAC + dynamic truncate
};

