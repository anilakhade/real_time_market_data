#include "websocket_client.h"
#include "logger.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace asio      = boost::asio;
namespace beast     = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

struct WebSocketClient::Impl {
    std::string url;
    Logger& log;
    Options opts;

    MessageCallback on_msg;
    StateCallback   on_state;
    std::function<void()> on_resub_noarg; // wrapper to invoke user ResubscribeFn

    asio::io_context ioc;
    asio::ssl::context ssl_ctx{asio::ssl::context::tls_client};
    std::unique_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>> ws;

    std::thread io_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> connected{false};

    Impl(std::string u, Logger& l, Options o)
        : url(std::move(u)), log(l), opts(std::move(o)) {}

    void notify_state(const std::string& s) {
        if (on_state) on_state(s);
        log.info("[ws] state=" + s);
    }

    static void parse_wss(const std::string& full, std::string& host, std::string& port, std::string& target) {
        const std::string scheme = "wss://";
        if (full.rfind(scheme, 0) != 0) throw std::runtime_error("WebSocketClient: only wss:// supported");
        std::string rest = full.substr(scheme.size());
        auto slash = rest.find('/');
        std::string hostport = (slash == std::string::npos) ? rest : rest.substr(0, slash);
        target = (slash == std::string::npos) ? "/" : rest.substr(slash);

        host = hostport; port = "443";
        if (auto colon = hostport.find(':'); colon != std::string::npos) {
            host = hostport.substr(0, colon);
            port = hostport.substr(colon + 1);
            if (port.empty()) port = "443";
        }
    }

    bool connect_once() {
        try {
            notify_state("connecting");

            std::string host, port, target;
            parse_wss(url, host, port, target);

            // TLS
            ssl_ctx.set_default_verify_paths();
            if (!opts.ca_file.empty()) ssl_ctx.load_verify_file(opts.ca_file);
            ssl_ctx.set_verify_mode(opts.verify_peer ? asio::ssl::verify_peer : asio::ssl::verify_none);

            // DNS + TCP connect
            tcp::resolver resolver(ioc);
            auto results = resolver.resolve(host, port);

            ws = std::make_unique<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(ioc, ssl_ctx);

            // Connect timeout at TCP layer
            beast::get_lowest_layer(*ws).expires_after(opts.conn_timeout);
            asio::connect(beast::get_lowest_layer(*ws).socket(), results.begin(), results.end());

            // SNI
            if (!SSL_set_tlsext_host_name(ws->next_layer().native_handle(), host.c_str()))
                throw std::runtime_error("SNI set failed");

            // TLS handshake
            ws->next_layer().handshake(asio::ssl::stream_base::client);

            // WS options + headers via decorator
            ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
            ws->set_option(websocket::stream_base::decorator([this](websocket::request_type& req) {
                for (const auto& kv : this->opts.headers) req.set(kv.first, kv.second);
            }));

            // WS handshake
            ws->handshake(host, target);
            ws->text(true);

            connected.store(true);
            notify_state("connected");
            return true;
        } catch (const std::exception& e) {
            log.error(std::string("[ws] connect failed: ") + e.what());
            connected.store(false);
            return false;
        }
    }

    void read_loop() {
        beast::flat_buffer buffer;
        while (running.load()) {
            try {
                buffer.clear();
                ws->read(buffer);
                if (on_msg) on_msg(beast::buffers_to_string(buffer.data()));
            } catch (const std::exception& e) {
                log.warn(std::string("[ws] read error: ") + e.what());
                connected.store(false);
                if (!running.load()) break;
                notify_state("reconnecting");
                reconnect_loop();
                break; // reconnect_loop re-enters read_loop() after success
            }
        }
    }

    void reconnect_loop() {
        auto backoff = opts.backoff_initial;
        while (running.load() && !connected.load()) {
            if (connect_once()) {
                if (on_resub_noarg) on_resub_noarg(); // user resubscribe hook
                read_loop();
                return;
            }
            std::this_thread::sleep_for(backoff);
            backoff = std::min(backoff * 2, opts.backoff_max);
        }
    }

    void run() {
        running.store(true);
        if (!connect_once()) {
            notify_state("reconnecting");
            reconnect_loop();
            return;
        }
        read_loop();
    }

    void stop() {
        running.store(false);
        try {
            if (ws && connected.load()) {
                beast::error_code ec;
                ws->close(websocket::close_code::normal, ec);
            }
        } catch (...) {}
        ioc.stop();
    }
};

// ---- public API ----

WebSocketClient::WebSocketClient(std::string wss_url, Logger& log)
    : WebSocketClient(std::move(wss_url), log, Options{}) {}

WebSocketClient::WebSocketClient(std::string wss_url, Logger& log, Options opts)
    : impl_(new Impl(std::move(wss_url), log, std::move(opts))),
      url_(impl_->url),
      log_(log),
      opts_(impl_->opts) {}

WebSocketClient::~WebSocketClient() {
    stop();
    delete impl_;
}

bool WebSocketClient::start() {
    if (impl_->running.load()) return true;
    impl_->io_thread = std::thread([this] { impl_->run(); });
    return true;
}

void WebSocketClient::stop() {
    if (!impl_->running.load()) return;
    impl_->stop();
    if (impl_->io_thread.joinable()) impl_->io_thread.join();
}

bool WebSocketClient::send_text(const std::string& payload) {
    if (!impl_->ws || !impl_->connected.load()) return false;
    try {
        impl_->ws->text(true);
        impl_->ws->write(asio::buffer(payload));
        return true;
    } catch (...) { return false; }
}

bool WebSocketClient::send_binary(const void* data, size_t len) {
    if (!impl_->ws || !impl_->connected.load()) return false;
    try {
        impl_->ws->binary(true);
        impl_->ws->write(asio::buffer(data, len));
        return true;
    } catch (...) { return false; }
}

void WebSocketClient::on_message(MessageCallback cb)   { impl_->on_msg = std::move(cb); }
void WebSocketClient::on_state(StateCallback cb)       { impl_->on_state = std::move(cb); }
void WebSocketClient::on_resubscribe(ResubscribeFn fn) {
    impl_->on_resub_noarg = [this, f = std::move(fn)]() mutable { if (f) f(*this); };
}

