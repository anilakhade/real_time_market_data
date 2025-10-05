// include/consumer.h
#pragma once
#include "ingest_queue.h"
#include "parser.h"
#include "ltp_store.h"
#include "logger.h"
#include <atomic>
#include <thread>
#include <functional>

class Consumer {
public:
    using SinkFn = std::function<void(const LTP&)>; // optional side-effect (print/persist)

    Consumer(IngestQueue& q, Parser& parser, LTPStore& store, Logger& log);
    ~Consumer();

    void set_sink(SinkFn fn);             // optional
    bool start();                         // spawn thread
    void stop();                          // join

private:
    void run();

    IngestQueue& q_;
    Parser& parser_;
    LTPStore& store_;
    Logger& log_;
    SinkFn sink_;

    std::atomic<bool> running_{false};
    std::thread thr_;
};

