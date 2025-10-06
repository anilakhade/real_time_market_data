#include "sharder.h"
#include "parser.h"
#include "ltp_store.h"
#include "logger.h"
#include <cassert>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <vector>
#include <iostream>

using namespace std::chrono_literals;

static std::string mk_ltp(const std::string& token, double px, long long ts_ms) {
    return std::string(R"({"data":{"token":")") + token + R"(","ltp":)" +
           std::to_string(px) + R"(,"exchange_timestamp":)" + std::to_string(ts_ms) + "}}";
}

static bool retry_broadcast(Sharder& mgr, const std::string& payload,
                            std::chrono::milliseconds timeout = 5s,
                            std::chrono::milliseconds step = 100ms) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (mgr.debug_broadcast_text(payload)) return true;
        std::this_thread::sleep_for(step);
    }
    return false;
}

int main() {
    Logger log("sharder_test");
    Parser parser; parser.set_strip_prefix("nse_cm|");
    LTPStore store;

    const char* env = std::getenv("WS_URL");
    std::string wss = env ? env : "wss://ws.ifelse.io";

    Sharder::Options opt;
    opt.wss_url = wss;
    opt.max_tokens_per_conn = 2;       // force 2 workers for 3 tokens
    opt.subscribe_batch_size = 2;
    opt.token_prefix = "nse_cm|";

    Sharder mgr(log, parser, store, opt);
    mgr.set_tokens({"26000","26001","26002"});

    assert(mgr.start());

    // Try broadcasting after connect; retry until a send succeeds (sockets may still be handshaking)
    const std::string p0 = mk_ltp("nse_cm|26000", 101.25, 1728123000000);
    const std::string p1 = mk_ltp("nse_cm|26001", 202.50, 1728123001000);

    bool s0 = retry_broadcast(mgr, p0, std::chrono::seconds(12));
    bool s1 = retry_broadcast(mgr, p1, std::chrono::seconds(12));

    assert(s0 && s1);

    // Wait for ingestion
    for (int i = 0; i < 200; ++i) { // up to ~10s
        if (store.size() >= 2) break;
        std::this_thread::sleep_for(50ms);
    }

    auto a = store.get("26000");
    auto b = store.get("26001");
    assert(a.has_value() && b.has_value());
    assert(a->ltp == 101.25);
    assert(b->ltp == 202.50);

    mgr.stop();
    std::cout << "Sharder test passed.\n";
    return 0;
}

