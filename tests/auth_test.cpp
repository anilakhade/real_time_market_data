#include "config.h"
#include "http_client.h"
#include "logger.h"
#include "auth.h"

#include <cstdlib>
#include <iostream>

int main() {
    try {
        Logger log("auth_test");
        auto cfg = Config::load_from_file("tests/config.json");
        HTTPClient http;

        Auth auth(cfg, http, log);

        // To run a real login, export SMARTAPI_TOTP=XXXXXX before running.
        if (const char* totp = std::getenv("SMARTAPI_TOTP")) {
            if (!auth.login_with_totp(totp)) {
                std::cerr << "Login failed\n";
                return 1;
            }
            auto hdrs = auth.auth_headers();
            std::cout << "Got JWT (len=" << auth.tokens().access_token.size() << ")\n";

            // Try refresh if desired (export SMARTAPI_REFRESH=1)
            if (std::getenv("SMARTAPI_REFRESH")) {
                if (!auth.refresh()) {
                    std::cerr << "Refresh failed\n";
                    return 1;
                }
                std::cout << "Refresh OK, JWT len=" << auth.tokens().access_token.size() << "\n";
            }
        } else {
            std::cout << "SMARTAPI_TOTP not set; dry-run only (compile/link test).\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "auth_test exception: " << e.what() << "\n";
        return 2;
    }
    return 0;
}

