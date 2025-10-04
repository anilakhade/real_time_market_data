#include "http_client.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <openssl/tls1.h>
#include <sstream>
#include <stdexcept>


using tcp = boost::asio::ip::tcp;
namespace http = boost::beast::http;


// ================ helper ====================
static std::string to_lower(std::string s){ for (auto& c:s) c=char(::tolower(c)); return s;}

static std::string map_get(const std::map<std::string, std::string>& m,
                           const std::string& k, const std::string& def="") {
    auto it=m.find(k); return it==m.end()?def:it->second;
}

static std::string url_encode(const std::string& s) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c=='_'|| c=='-'||c=='.'||c=='~') out.push_back(c);
        else {out.push_back('%'); out.push_back(hex[c>>4]); out.push_back(hex[c&15]);}
    }
    return out;
}

// ================= HTTPClient ===================
HTTPClient::HTTPClient() : opts_{} {}
HTTPClient::HTTPClient(Options opts) : opts_(std::move(opts)) {}

void HTTPClient::set_default_header(const std::string& k, const std::string& v) {
    opts_.default_headers[k] = v;
}

void HTTPClient::erase_default_header(const std::string& k) {
    opts_.default_headers.erase(k);
}

std::string HTTPClient::build_query_string(const std::map<std::string, std::string>& q){
    if (q.empty()) return {};
    std::ostringstream oss; bool first=true;
    for (auto& [k,v] : q) {
        if (!first) oss << '&'; first=false;
        oss << url_encode(k) << '=' << url_encode(v);
    }
    return oss.str();
}

HTTPClient::UrlParts HTTPClient::parse_https_url(
    const std::string& https_url,
    const std::map<std::string, std::string>& query)
{
    const std::string scheme = "https://";
    if (https_url.rfind(scheme,0)!=0)
        throw std::runtime_error("HTTPClient: only https:// URLs supported");

    auto rest = https_url.substr(scheme.size());
    auto slash = rest.find('/');
    std::string hostport = slash==std::string::npos ? rest : rest.substr(0,slash);
    std::string path     = slash==std::string::npos ? "/" : rest.substr(slash);

    std::string host = hostport;
    std::string port = "443";

    if (auto colon = hostport.find(':'); colon!=std::string::npos) {
        host = hostport.substr(0, colon);
        port = hostport.substr(colon+1);
        if (port.empty()) port = "443";
    }

    // merge query 
    std::string qs = build_query_string(query);
    if (!qs.empty()){
        if (auto qpos = path.find('?'); qpos==std::string::npos) path += '?' + qs;
        else path += '&' + qs;
    }

    return {host, port, path};
}

static void apply_headers(http::request<http::string_body>& req,
                          const HTTPClient::Options& opts,
                          const std::map<std::string, std::string>& headers)
{
    req.set(http::field::user_agent, opts.user_agent);
    for (auto& [k, v] : opts.default_headers) req.set(k, v);
    for (auto& [k, v] : headers) req.set(k, v);
}

static HttpResponse perform_request(const HTTPClient::Options& opts,
                                    const std::string& host,
                                    const std::string& port,
                                    http::request<http::string_body>& req)
{
    boost::asio::io_context ioc;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

    ssl_ctx.set_default_verify_paths();
    if (!opts.ca_file.empty())
        ssl_ctx.load_verify_file(opts.ca_file);
    ssl_ctx.set_verify_mode(opts.verify_peer
                            ? boost::asio::ssl::verify_peer
                            : boost::asio::ssl::verify_none);

    tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(host, port);

    boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ssl_ctx);

    //timeouts
    stream.next_layer().expires_after(opts.timeout);

    //connect
    boost::asio::connect(stream.next_layer().socket(), results.begin(), results.end());

    // TLS handshake
    if (! SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
        throw std::runtime_error("SNI set failed");
    stream.handshake(boost::asio::ssl::stream_base::client);

    //send 
    req.set(http::field::host, host);
    http::write(stream, req);

    //receive
    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    // shutdown (best effort)
    boost::system::error_code ec;
    stream.shutdown(ec);

    HttpResponse out;
    out.status = static_cast<int>(res.result_int());
    out.body   = std::move(res.body());
    for (auto const& f: res.base()) {
        out.headers.emplace(std::string(f.name_string()), std::string(f.value()));
    }
    return out;
}

HttpResponse HTTPClient::get(const std::string& https_url,
                             const std::map<std::string, std::string>& headers,
                             const std::map<std::string, std::string>& query)
{
    auto u = parse_https_url(https_url, query);
    http::request<http::string_body> req{http::verb::get, u.target, 11};
    apply_headers(req, opts_, headers);
    return perform_request(opts_, u.host, u.port, req);
}

HttpResponse HTTPClient::post(const std::string& https_url,
                              const std::string& body,
                              const std::string& content_type,
                              const std::map<std::string, std::string>& headers,
                              const std::map<std::string, std::string>& query)
{
    auto u = parse_https_url(https_url, query);
    http::request<http::string_body> req{http::verb::post, u.target, 11};
    req.body() = body;
    req.prepare_payload();
    req.set(http::field::content_type, content_type);
    apply_headers(req, opts_, headers);
    return perform_request(opts_, u.host, u.port, req);
}

HttpResponse HTTPClient::post_json(const std::string& https_url,
                                   const std::string& json_body,
                                   const std::map<std::string, std::string>& headers,
                                   const std::map<std::string, std::string>& query)
{
    auto h = headers;
    if (!h.count("Content-Type")) h["Content-Type"] = "application/json";
    return post(https_url, json_body, "application/json", h, query);
}
































