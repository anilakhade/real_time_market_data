#include "consumer.h"
#include "ingest_queue.h"
#include "parser.h"
#include "ltp_store.h"
#include "logger.h"
#include <cassert>
#include <chrono>
#include <thread>
#include <optional>

static std::string mk_msg(const std::string& token, double px, long long ts_ms) {
    return std::string(R"({"data":{"token":")") + token + R"(","ltp":)" +
           std::to_string(px) + R"(,"exchange_timestamp":)" + std::to_string(ts_ms) + "}}";
}

int main() {
    Logger log("consumer_test");
    IngestQueue q(64);
    Parser p; p.set_strip_prefix("nse_cm|");
    LTPStore store;

    Consumer c(q, p, store, log);
    c.set_sink([&](const LTP& v){ /* optional: log.info_fmt("ingested ", v.token, " ", v.ltp); */ });
    c.start();

    // push a few frames
    assert(q.try_push(mk_msg("nse_cm|26000", 101.5, 1728123000000)));
    assert(q.try_push(mk_msg("nse_cm|26001", 202.25, 1728123001000)));
    assert(q.try_push(mk_msg("nse_cm|26000", 103.0, 1728123002000))); // update

    // wait for consumer to process
    using namespace std::chrono_literals;
    for (int i = 0; i < 50; ++i) {
        if (store.size() >= 2) break;
        std::this_thread::sleep_for(10ms);
    }

    auto a = store.get("26000");
    auto b = store.get("26001");
    assert(a.has_value() && b.has_value());
    assert(a->ltp == 103.0);     // last update applied
    assert(b->ltp == 202.25);

    c.stop();
    std::cout << "Consumer/LTPStore test passed.\n";
    return 0;
}

