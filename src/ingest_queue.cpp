#include "ingest_queue.h"
#include <cassert>
#include <utility>

static inline bool is_power_of_two(std::size_t x) { return x && ((x & (x - 1)) == 0); }

std::size_t IngestQueue::next_pow2(std::size_t n) noexcept {
    if (n < 8) return 8;
    if (is_power_of_two(n)) return n;
    n--;
    for (std::size_t i = 1; i < sizeof(std::size_t) * 8; i <<= 1) n |= (n >> i);
    return n + 1;
}

IngestQueue::IngestQueue(std::size_t capacity)
    : buf_(next_pow2(capacity)), mask_(buf_.size() - 1) {
    assert(is_power_of_two(buf_.size()));
}

bool IngestQueue::try_push(std::string&& msg) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (head - tail == capacity()) return false; // full
    buf_[head & mask_] = std::move(msg);
    head_.store(head + 1, std::memory_order_release);
    return true;
}

bool IngestQueue::try_push(const std::string& msg) {
    std::string tmp = msg; // copy once; move into slot
    return try_push(std::move(tmp));
}

bool IngestQueue::try_pop(std::string& out) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t head = head_.load(std::memory_order_acquire);
    if (head == tail) return false; // empty
    out = std::move(buf_[tail & mask_]);
    tail_.store(tail + 1, std::memory_order_release);
    return true;
}

std::size_t IngestQueue::size() const noexcept {
    const std::size_t head = head_.load(std::memory_order_acquire);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    return head - tail;
}

bool IngestQueue::empty() const noexcept {
    return size() == 0;
}

bool IngestQueue::full() const noexcept {
    return size() == capacity();
}

void IngestQueue::clear() noexcept {
    // Only call when producer/consumer paused.
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
    for (auto& s : buf_) s.clear();
}

