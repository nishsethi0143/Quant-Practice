#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <new> // Required for hardware_destructive_interference_size

using namespace std;

struct Price {
    atomic<long long> ticks{0};
};

// SCENARIO 1: Tight layout where call/put feeds may share cache lines.
struct SharedLineBook {
    Price call_bid;
    Price put_bid;
};

// SCENARIO 2: Manual 64-byte alignment.
struct CacheAlignedBook64 {
    alignas(64) Price call_bid;
    alignas(64) Price put_bid;
};

// SCENARIO 3: Hardware-aware separation to reduce destructive interference.
struct CacheAlignedBookHw {
    alignas(std::hardware_destructive_interference_size) Price call_bid;
    alignas(std::hardware_destructive_interference_size) Price put_bid;
};

template <typename Book>
long long run_feed_benchmark(Book& book) {
    auto start = chrono::high_resolution_clock::now();

    jthread call_feed([&] {
        for (int i = 0; i < 10000000; ++i) {
            book.call_bid.ticks.fetch_add(1, memory_order_relaxed);
        }
    });
    jthread put_feed([&] {
        for (int i = 0; i < 10000000; ++i) {
            book.put_bid.ticks.fetch_add(1, memory_order_relaxed);
        }
    });

    call_feed.join();
    put_feed.join();
    auto end = chrono::high_resolution_clock::now();
    return chrono::duration_cast<chrono::milliseconds>(end - start).count();
}

void benchmark_false_sharing() {
    SharedLineBook shared;
    long long shared_ms = run_feed_benchmark(shared);
    cout << "Problem 102/104 - Shared Line Book: " << shared_ms << "ms\n";

    CacheAlignedBook64 aligned64;
    long long aligned64_ms = run_feed_benchmark(aligned64);
    cout << "Problem 102/104 - alignas(64): " << aligned64_ms << "ms\n";

    CacheAlignedBookHw aligned_hw;
    long long aligned_hw_ms = run_feed_benchmark(aligned_hw);
    cout << "Problem 102/104 - hardware_destructive_interference_size: " << aligned_hw_ms << "ms\n";

    const long long cp_imbalance_ticks =
        aligned_hw.call_bid.ticks.load(memory_order_relaxed) -
        aligned_hw.put_bid.ticks.load(memory_order_relaxed);
    cout << "Final (C-P) tick imbalance snapshot: " << cp_imbalance_ticks << "\n";
}

int main() {
    benchmark_false_sharing();
    return 0;
}