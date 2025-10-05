#include "totp.h"

#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

// Base32 alphabet per RFC 4648
int b32_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '2' && c <= '7') return 26 + (c - '2');
    return -1;
}

std::string base32_decode_impl(const std::string& in_raw) {
    // Normalize: uppercase, strip spaces and '=' padding
    std::string in;
    in.reserve(in_raw.size());
    for (char c : in_raw) {
        if (c == '=') continue;
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        in.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    std::string out;
    out.reserve(in.size() * 5 / 8 + 1);

    int buffer = 0;
    int bits_left = 0;

    for (char c : in) {
        int v = b32_val(c);
        if (v < 0) throw std::runtime_error("TOTP: invalid Base32 character");
        buffer = (buffer << 5) | v;
        bits_left += 5;
        if (bits_left >= 8) {
            bits_left -= 8;
            unsigned char byte = static_cast<unsigned char>((buffer >> bits_left) & 0xFF);
            out.push_back(static_cast<char>(byte));
        }
    }
    // leftover bits are ignored per RFC 4648 unless padding mandated; we accept unpadded input.
    return out;
}

const EVP_MD* md_for_algo(TOTPAlgo algo) {
    switch (algo) {
        case TOTPAlgo::SHA1:   return EVP_sha1();
        case TOTPAlgo::SHA256: return EVP_sha256();
        case TOTPAlgo::SHA512: return EVP_sha512();
        default: return EVP_sha1();
    }
}

std::string left_pad_int(uint32_t val, int digits) {
    uint32_t mod = 1;
    for (int i = 0; i < digits; ++i) mod *= 10;
    uint32_t code = val % mod;

    std::ostringstream oss;
    oss << std::setw(digits) << std::setfill('0') << code;
    return oss.str();
}

} // namespace

// ----------- TOTP public API -----------

TOTP::TOTP(std::string secret_base32,
           int digits,
           std::chrono::seconds period,
           TOTPAlgo algo)
    : secret_(base32_decode(secret_base32)),
      digits_(digits),
      period_(period),
      algo_(algo)
{
    if (digits_ < 6 || digits_ > 10) {
        throw std::invalid_argument("TOTP: digits must be between 6 and 10");
    }
    if (period_.count() <= 0) {
        throw std::invalid_argument("TOTP: period must be positive");
    }
}

std::string TOTP::base32_decode(const std::string& b32) {
    return base32_decode_impl(b32);
}

uint64_t TOTP::time_counter(std::chrono::system_clock::time_point tp,
                            std::chrono::seconds period) {
    using namespace std::chrono;
    auto secs = duration_cast<seconds>(tp.time_since_epoch()).count();
    return static_cast<uint64_t>(secs >= 0 ? secs : 0) / static_cast<uint64_t>(period.count());
}

std::string TOTP::hotp(const std::string& key,
                       uint64_t counter,
                       int digits,
                       TOTPAlgo algo)
{
    // counter in big-endian 8 bytes
    std::array<unsigned char, 8> msg{};
    for (int i = 7; i >= 0; --i) {
        msg[i] = static_cast<unsigned char>(counter & 0xFF);
        counter >>= 8;
    }

    unsigned int len = EVP_MD_size(md_for_algo(algo));
    std::array<unsigned char, EVP_MAX_MD_SIZE> mac{};

    if (!HMAC(md_for_algo(algo),
              reinterpret_cast<const unsigned char*>(key.data()), static_cast<int>(key.size()),
              msg.data(), msg.size(),
              mac.data(), &len)) {
        throw std::runtime_error("TOTP: HMAC failed");
    }

    // dynamic truncation (RFC 4226)
    int offset = mac[len - 1] & 0x0F;
    uint32_t bin_code =
        ((mac[offset]   & 0x7F) << 24) |
        ((mac[offset+1] & 0xFF) << 16) |
        ((mac[offset+2] & 0xFF) <<  8) |
        ((mac[offset+3] & 0xFF) <<  0);

    return left_pad_int(bin_code, digits);
}

std::string TOTP::code_at(std::chrono::system_clock::time_point tp) const {
    const uint64_t ctr = time_counter(tp, period_);
    return hotp(secret_, ctr, digits_, algo_);
}

std::string TOTP::now() const {
    return code_at(std::chrono::system_clock::now());
}

bool TOTP::verify(const std::string& code,
                  std::chrono::system_clock::time_point tp,
                  int window_steps) const
{
    const uint64_t ctr = time_counter(tp, period_);
    if (code_at(tp) == code) return true;
    for (int w = 1; w <= window_steps; ++w) {
        if (hotp(secret_, ctr + w, digits_, algo_) == code) return true;
        if (ctr >= static_cast<uint64_t>(w) &&
            hotp(secret_, ctr - w, digits_, algo_) == code) return true;
    }
    return false;
}

