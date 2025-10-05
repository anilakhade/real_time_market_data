#include "consumer.h"

Consumer::Consumer(IngestQueue& q, Parser& parser, LTPStore& store, Logger& log)
    : q_(q), parser_(parser), store_(store), log_(log) {}

Consumer::~Consumer() { stop(); }

void Consumer::set_sink(SinkFn fn) { sink_ = std::move(fn); }

bool Consumer::start() {
    if (running_.exchange(true)) return true;
    thr_ = std::thread([this]{ run(); });
    return true;
}

void Consumer::stop() {
    if (!running_.exchange(false)) return;
    if (thr_.joinable()) thr_.join();
}

void Consumer::run() {
    std::string msg;
    while (running_.load()) {
        if (!q_.try_pop(msg)) {
            std::this_thread::yield();
            continue;
        }
        auto ltp = parser_.parse_ltp(msg);
        if (!ltp) continue;
        store_.upsert(*ltp);
        if (sink_) sink_(*ltp);
    }
}

