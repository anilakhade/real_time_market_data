#include "totp.h"
#include <iostream>
#include <cstdlib>
#include <chrono>

int main() {
    try {
        // --- RFC 6238 test vector (SHA1, 8 digits, 30s) ---
        // Secret "12345678901234567890" in Base32 = GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ
        // At T = 59 -> 94287082
        {
            TOTP totp("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", 8, std::chrono::seconds(30), TOTPAlgo::SHA1);
            auto t = std::chrono::system_clock::time_point(std::chrono::seconds(59));
            const std::string code = totp.code_at(t);
            if (code != "94287082") {
                std::cerr << "TOTP RFC test failed: got " << code << " expected 94287082\n";
                return 1;
            }
        }

        // Optional live demo using your secret:
        if (const char* sec = std::getenv("SMARTAPI_TOTP_SECRET")) {
            TOTP my(sec, 6, std::chrono::seconds(30), TOTPAlgo::SHA1);
            std::cout << "Current TOTP: " << my.now() << "\n";
        } else {
            std::cout << "Set SMARTAPI_TOTP_SECRET to print a live code.\n";
        }

        std::cout << "TOTP test passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "totp_test exception: " << e.what() << "\n";
        return 2;
    }
}

