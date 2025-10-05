// tests/subscription_manager_test.cpp
#include "subscription_manager.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <iostream>
#include <unordered_set>
#include <string>
#include <vector>

using nlohmann::json;

static std::unordered_set<std::string> to_set(const json& arr) {
    std::unordered_set<std::string> s;
    for (const auto& v : arr) s.insert(v.get<std::string>());
    return s;
}
static std::string strip_prefix(const std::string& s, const std::string& prefix) {
    return (s.rfind(prefix, 0) == 0) ? s.substr(prefix.size()) : s;
}

int main() {
    Logger log("subman_test");
    const std::string prefix = "nse_cm|";

    SubscriptionManager sm(
        log,
        SubscriptionManager::Mode::LTP,
        /*batch_size=*/2,
        [prefix](const std::string& t){ return prefix + t; }
    );

    // desired = {A,B,C}
    sm.add_many({"A","B","C"});

    // Build subscribe batches (order not guaranteed). Expect 2 batches total.
    auto subs = sm.build_subscribe_batches();
    assert(subs.size() == 2);

    auto j0 = json::parse(subs[0]);
    auto j1 = json::parse(subs[1]);

    assert(j0["action"] == "subscribe"); assert(j0["mode"] == "ltp");
    assert(j1["action"] == "subscribe"); assert(j1["mode"] == "ltp");

    // Union tokens across both batches must be {prefix+A, prefix+B, prefix+C}.
    auto s0 = to_set(j0["tokens"]);
    auto s1 = to_set(j1["tokens"]);
    std::unordered_set<std::string> all = s0; all.insert(s1.begin(), s1.end());
    assert(all.size() == 3);
    assert(all.count(prefix + "A"));
    assert(all.count(prefix + "B"));
    assert(all.count(prefix + "C"));

    // Simulate server ACK for the batch that has size 2 (mark ACTIVE using RAW tokens).
    std::vector<std::string> active_raw;
    if (j0["tokens"].size() == 2) {
        active_raw.push_back(strip_prefix(j0["tokens"][0].get<std::string>(), prefix));
        active_raw.push_back(strip_prefix(j0["tokens"][1].get<std::string>(), prefix));
    } else {
        active_raw.push_back(strip_prefix(j1["tokens"][0].get<std::string>(), prefix));
        active_raw.push_back(strip_prefix(j1["tokens"][1].get<std::string>(), prefix));
    }
    sm.mark_subscribed(active_raw);

    // Now only one token should remain pending subscribe → exactly one batch with one token.
    auto subs2 = sm.build_subscribe_batches();
    assert(subs2.size() == 1);
    auto j2 = json::parse(subs2[0]);
    assert(j2["action"] == "subscribe"); assert(j2["mode"] == "ltp");
    assert(j2["tokens"].size() == 1);
    const std::string remaining_prefixed = j2["tokens"][0].get<std::string>();
    assert(all.count(remaining_prefixed));
    const std::string remaining_raw = strip_prefix(remaining_prefixed, prefix);

    // Trigger an unsubscribe by removing ONE of the ACTIVE tokens from desired.
    // (Removing the remaining/pending one would NOT create an unsubscribe.)
    const std::string to_remove_raw = active_raw[0];
    sm.remove(to_remove_raw);

    // Expect one unsubscribe batch for that exact prefixed token we just removed.
    auto unsubs = sm.build_unsubscribe_batches();
    assert(unsubs.size() == 1);
    auto ju = json::parse(unsubs[0]);
    assert(ju["action"] == "unsubscribe"); assert(ju["mode"] == "ltp");
    assert(ju["tokens"].size() == 1);
    assert(ju["tokens"][0] == prefix + to_remove_raw);

    std::cout << "SubscriptionManager test passed.\n";
    return 0;
}

