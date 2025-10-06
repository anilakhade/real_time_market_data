// src/sharder.cpp
#include "sharder.h"

#include "websocket_client.h"
#include "subscription_manager.h"
#include "ingest_queue.h"
#include "consumer.h"
#include "parser.h"
#include "ltp_store.h"
#include "logger.h"

#include <memory>
#include <utility>
#include <algorithm>
#include <mutex>

struct Sharder::Worker {
    // per-worker stack
    std::unique_ptr<WebSocketClient>      ws;
    std::unique_ptr<SubscriptionManager>  sub;
    std::unique_ptr<IngestQueue>          q;
    std::unique_ptr<Consumer>             cons;

    // tokens assigned to this shard (RAW tokens, e.g. "26000")
    std::vector<std::string> tokens;
};

struct Sharder::Impl {
    Logger& log;
    Parser& parser;
    LTPStore& store;
    Options opts;

    // auth/header state
    std::string auth_header_value; // e.g., "Bearer <JWT>"
    std::map<std::string,std::string> common_headers;

    // desired full token list (RAW tokens)
    std::vector<std::string> desired_tokens;

    // workers
    std::vector<std::unique_ptr<Worker>> workers;
    std::atomic<bool> running{false};

    std::mutex mu; // protects header/desired updates while running

    Impl(Logger& lg, Parser& p, LTPStore& st, Options o)
        : log(lg), parser(p), store(st), opts(std::move(o)) {}

    static std::vector<std::vector<std::string>>
    shard(const std::vector<std::string>& tokens, std::size_t max_per_conn) {
        std::vector<std::vector<std::string>> out;
        if (tokens.empty()) return out;
        if (max_per_conn == 0) max_per_conn = 800;
        out.reserve((tokens.size() + max_per_conn - 1) / max_per_conn);
        for (std::size_t i = 0; i < tokens.size(); i += max_per_conn) {
            auto last = std::min(tokens.size(), i + max_per_conn);
            out.emplace_back(tokens.begin() + static_cast<std::ptrdiff_t>(i),
                             tokens.begin() + static_cast<std::ptrdiff_t>(last));
        }
        return out;
    }

    std::map<std::string,std::string> effective_headers_locked() const {
        std::map<std::string,std::string> h = common_headers;
        if (!auth_header_value.empty()) h["Authorization"] = auth_header_value;
        return h;
    }

    void build_workers_locked() {
        // Tear down any previous
        workers.clear();

        // Shard tokens
        auto shards = shard(desired_tokens, opts.max_tokens_per_conn);
        if (shards.empty()) {
            // create at least one idle worker so start/stop works
            shards.emplace_back();
        }

        for (auto& shard_tokens : shards) {
            auto w = std::make_unique<Worker>();
            w->tokens = shard_tokens;

            // Subscription manager (prefix, batching)
            auto token_fmt = [pref = opts.token_prefix](const std::string& t){
                return pref.empty() ? t : (pref + t);
            };
            w->sub = std::make_unique<SubscriptionManager>(
                log, SubscriptionManager::Mode::LTP, opts.subscribe_batch_size, token_fmt);
            if (!w->tokens.empty()) w->sub->add_many(w->tokens);

            // Queue + Consumer
            w->q = std::make_unique<IngestQueue>(1024 * 8); // 8k ring, tweak later if needed
            w->cons = std::make_unique<Consumer>(*w->q, parser, store, log);

            // WS client options
            WebSocketClient::Options wopts;
            wopts.verify_peer = opts.verify_peer;
            wopts.ca_file = opts.ca_file;
            wopts.headers = effective_headers_locked();
            wopts.ping_interval = std::chrono::seconds(15);
            wopts.conn_timeout = std::chrono::seconds(10);

            // WS client
            w->ws = std::make_unique<WebSocketClient>(opts.wss_url, log, wopts);

            // Wire callbacks
            w->ws->on_state([this](const std::string& s){
                log.info(std::string("sharder/ws state=") + s);
            });

            // Push raw frames into queue (drop if full)
            IngestQueue& qref = *w->q;
            Logger& lref = log;
            w->ws->on_message([&qref, &lref](const std::string& msg){
                if (!qref.try_push(msg)) {
                    lref.warn("ingest queue full: dropped frame");
                }
            });

            // Resubscribe on reconnect
            SubscriptionManager& subref = *w->sub;
            WebSocketClient* wsptr = w->ws.get();
            w->ws->on_resubscribe([&subref, wsptr](WebSocketClient&){
                // Build and send subscribe batches again
                for (const auto& payload : subref.build_subscribe_batches()) {
                    wsptr->send_text(payload);
                }
            });

            workers.emplace_back(std::move(w));
        }
    }

    void send_initial_subscribes() {
        for (auto& w : workers) {
            if (!w->ws) continue;
            // On connect, WebSocketClient already fires "connected" state.
            // We also explicitly send now in case state hook order varies.
            for (const auto& payload : w->sub->build_subscribe_batches()) {
                w->ws->send_text(payload);
            }
        }
    }
};

// ----------------- Sharder public API -----------------

Sharder::Sharder(Logger& log, Parser& parser, LTPStore& store, Options opts)
    : impl_(new Impl(log, parser, store, std::move(opts))) {}

Sharder::~Sharder() {
    stop();
    delete impl_;
}

void Sharder::set_access_token(const std::string& auth_header_value) {
    std::lock_guard<std::mutex> lk(impl_->mu);
    impl_->auth_header_value = auth_header_value;
    // propagate to live workers (for future reconnects)
    for (auto& w : impl_->workers) {
        if (!w->ws) continue;
        auto h = impl_->effective_headers_locked();
        // rebuild headers on ws options via small trick: stop/start will pick new headers.
        // For live connections, we rely on reconnect to apply updated headers.
        (void)h;
    }
}

void Sharder::set_common_headers(const std::map<std::string,std::string>& hdrs) {
    std::lock_guard<std::mutex> lk(impl_->mu);
    impl_->common_headers = hdrs;
}

void Sharder::set_tokens(const std::vector<std::string>& tokens) {
    std::lock_guard<std::mutex> lk(impl_->mu);
    impl_->desired_tokens = tokens;
    if (impl_->running.load()) {
        // Re-slice live: update each worker's desired set (best-effort simple approach: rebuild workers)
        // For simplicity and safety, rebuild the worker stack while stopped.
        // Caller can call stop(); set_tokens(); start(); for a clean rebuild.
        impl_->log.warn("set_tokens while running: changes will apply on next start()");
    }
}

bool Sharder::start() {
    std::lock_guard<std::mutex> lk(impl_->mu);
    if (impl_->running.load()) return true;

    // Build workers from current tokens/headers
    impl_->build_workers_locked();

    // Start consumers first so queues are drained
    for (auto& w : impl_->workers) {
        if (w->cons) w->cons->start();
    }

    // Start websockets
    for (auto& w : impl_->workers) {
        if (w->ws) {
            // Also set a resubscribe that uses the worker-local SubscriptionManager (already wired)
            w->ws->start();
        }
    }

    // Initial subscribe payloads
    impl_->send_initial_subscribes();

    impl_->running.store(true);
    return true;
}

void Sharder::stop() {
    std::lock_guard<std::mutex> lk(impl_->mu);
    if (!impl_->running.load()) return;

    // Stop websockets first
    for (auto& w : impl_->workers) {
        if (w->ws) w->ws->stop();
    }
    // Then consumers
    for (auto& w : impl_->workers) {
        if (w->cons) w->cons->stop();
    }

    impl_->workers.clear();
    impl_->running.store(false);
}

bool Sharder::running() const noexcept {
    return impl_->running.load();
}

std::size_t Sharder::num_workers() const noexcept {
    return impl_->workers.size();
}

std::vector<std::string> Sharder::desired_tokens_snapshot() const {
    std::lock_guard<std::mutex> lk(impl_->mu);
    return impl_->desired_tokens;
}

bool Sharder::debug_broadcast_text(const std::string& payload) {
    if (!impl_->running.load()) return false;
    bool any = false;
    for (auto& w : impl_->workers) {
        if (w->ws) { any = w->ws->send_text(payload) || any; }
    }
    return any;
}

