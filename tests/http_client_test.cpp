#include "http_client.h"
#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    try {
        HTTPClient client;

        // Get test
        auto r1 = client.get("https://httpbin.org/get", {}, {{"ping", "1"}});
        if (r1.status/100 != 2) { std::cerr << "GET status " << r1.status << "\n"; return 1;}
        auto j1 = nlohmann::json::parse(r1.body);
        if (j1.at("args").at("ping") != "1") {std::cerr << "GET args mismatch\n"; return 2;}

        // post json test
        auto r2 = client.post_json("https://httpbin.org/post", R"({"x":42})");
        if (r2.status/100 != 2) { std::cerr << "POST status " << r2.status << "\n"; return 3;}
        auto j2 = nlohmann::json::parse(r2.body);
        if (j2.at("json").at("x") != 42) { std::cerr << "POST json mismatch\n"; return 4;}

        std::cout << "HTTPClient test passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 10;
    }
}
