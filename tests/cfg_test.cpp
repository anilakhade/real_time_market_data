#include "config.h"
#include <iostream>


int main() {
    try {
        Config cfg = Config::load_from_file("../tests/config.json");

        if (cfg.api_key() != "example_api_key_123"){
            std::cerr << "api_key mismatch\n";
            return 2;
        }
        if (cfg.token().size() != 3) {
            std::cerr << "tokens size mismatch\n";
            return 2;
        }
        if (cfg.splits().at("RELIANCE") != 1) {
            std::cerr << "splits mismatch\n";
            return 2;
        }
        
        std::cout << "Config load test passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}

