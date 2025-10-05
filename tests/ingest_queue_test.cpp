#include "ingest_queue.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <string>

int main() {
    // Small capacity to exercise wrap & full conditions (rounded to next pow2).
    IngestQueue q(8);

    // Edge: empty pop
    std::string tmp;
    assert(!q.try_pop(tmp));
    assert(q.empty());

    // Single-thread push/pop basic
    assert(q.try_push("a"));
    assert(q.try_push("b"));
    assert(q.size() == 2);
    assert(q.try_pop(tmp) && tmp == "a");
    assert(q.try_pop(tmp) && tmp == "b");
    assert(q.empty());

    // Fill to full
    for (int i = 0; i < static_cast<int>(q.capacity()); ++i) {
        bool ok = q.try_push(std::to_string(i));
        assert(ok);
    }
    assert(q.full());
    // One more must fail
    assert(!q.try_push("x"));

    // Drain all
    for (std::size_t i = 0; i < q.capacity(); ++i) {
        bool ok = q.try_pop(tmp);
        assert(ok);
    }
    assert(q.empty());

    // SPSC threaded test
    const int N = 10000;
    IngestQueue q2(1024);

    std::thread prod([&]{
        for (int i = 0; i < N; ) {
            if (q2.try_push(std::to_string(i))) ++i;
            else std::this_thread::yield();
        }
    });

    int got = 0;
    std::thread cons([&]{
        std::string s;
        while (got < N) {
            if (q2.try_pop(s)) {
                // verify monotonic increasing sequence
                assert(std::stoi(s) == got);
                ++got;
            } else {
                std::this_thread::yield();
            }
        }
    });

    prod.join();
    cons.join();
    assert(q2.empty());

    std::cout << "IngestQueue test passed.\n";
    return 0;
}

