// include/ingest_queue.h
#pragma once
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

class IngestQueue {
public:
    // capacity will be rounded up to next power of two (min 8)
    explicit IngestQueue(std::size_t capacity);

    IngestQueue(const IngestQueue&) = delete;
    IngestQueue& operator=(const IngestQueue&) = delete;
    IngestQueue(IngestQueue&&) = delete;
    IngestQueue& operator=(IngestQueue&&) = delete;

    // Producer thread (WebSocket read loop)
    // Returns false if queue is full (item dropped upstream)
    bool try_push(std::string&& msg);
    bool try_push(const std::string& msg); // convenience (copies)

    // Consumer thread (Parser)
    // Returns false if queue is empty
    bool try_pop(std::string& out);

    // Introspection (non-blocking)
    std::size_t size() const noexcept;     // approximate (lock-free)
    std::size_t capacity() const noexcept { return mask_ + 1; }
    bool empty() const noexcept;
    bool full() const noexcept;

    // Reset (only safe when both threads paused)
    void clear() noexcept;

private:
    // power-of-two ring: index & mask_ for wrap
    std::vector<std::string> buf_;
    const std::size_t mask_;             // capacity - 1

    // head_ (write index) modified by producer only
    // tail_ (read index)  modified by consumer only
    // Both atomics so the opposite side can observe progress.
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};

    static std::size_t next_pow2(std::size_t n) noexcept;
};

