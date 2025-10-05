#include "websocket_client.h"
#include "logger.h"

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

int main() {
    Logger log("ws_test");
    // Public echo WSS (override via WS_URL env if you want)
    const char* env = std::getenv("WS_URL");
    std::string url = env ? env : "wss://echo.websocket.events";

    WebSocketClient::Options opts{};
    WebSocketClient ws(url, log, opts);

    std::promise<std::string> got;
    auto fut = got.get_future();

    ws.on_state([&](const std::string& s){
        log.info("state=" + s);
        if (s == "connected") {
            ws.send_text("hello");
        }
    });

    ws.on_message([&](const std::string& msg){
        log.info("recv: " + msg);
        // echo server sometimes sends a greeting first; only fulfill on our echo
        if (msg == "hello") {
            if (fut.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
                got.set_value(msg);
        }
    });

    ws.start();

    // Wait up to 5s for the echo
    if (fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        std::cerr << "WebSocket echo test: timeout waiting for echo\n";
        ws.stop();
        return 1;
    }

    const auto echoed = fut.get();
    if (echoed != "hello") {
        std::cerr << "WebSocket echo test: unexpected payload: " << echoed << "\n";
        ws.stop();
        return 2;
    }

    std::cout << "WebSocket echo test passed.\n";
    ws.stop();
    return 0;
}

