#include "logger.h"
#include <thread>
#include <vector>
#include <iostream>

int main() {
    Logger log("logger_test");
    log.set_level(LogLevel::DEBUG);

    const int threads = 4;
    const int msgs = 200;

    std::vector<std::thread> th;
    for (int t = 0; t < threads; ++t) {
        th.emplace_back([t, msgs, &log](){
            for (int i = 0; i < msgs; ++i) {
                std::ostringstream ss;
                ss << "threads=" << t << " msg=" << i;
                log.debug(ss.str());
            }
        });
    }
    for (auto &x : th) x.join();

    std::cout << "logger test finished\n";
    return 0;
}
