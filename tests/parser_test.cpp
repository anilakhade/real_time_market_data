#include "parser.h"
#include <cassert>
#include <iostream>

static std::string ms_payload() {
    return R"({
        "data": {
            "token": "nse_cm|26000",
            "ltp": 123.45,
            "exchange_timestamp": 1728123456789
        }
    })";
}

static std::string sec_payload() {
    return R"({
        "symbol": "26001",
        "last_price": "101.5",
        "timestamp": 1728123456
    })";
}

static std::string bad_payload() {
    return R"({"foo":1,"bar":2})";
}

int main() {
    Parser p;
    p.set_strip_prefix("nse_cm|");

    // ms payload
    auto a = p.parse_ltp(ms_payload());
    assert(a.has_value());
    assert(a->token == "26000");
    assert(a->ltp == 123.45);

    // sec payload (no prefix to strip)
    auto b = p.parse_ltp(sec_payload());
    assert(b.has_value());
    assert(b->token == "26001");
    assert(b->ltp == 101.5);

    // bad payload
    auto c = p.parse_ltp(bad_payload());
    assert(!c.has_value());

    std::cout << "Parser test passed.\n";
    return 0;
}

