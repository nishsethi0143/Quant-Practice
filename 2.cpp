#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <new> // Required for hardware_destructive_interference_size

using namespace std;

// SCENARIO 1: Close together (Will cause False Sharing)
struct BadLayout {
    alignas(8) atomic<long long> count1{0};
    alignas(8) atomic<long long> count2{0}; 
};

// SCENARIO 2: Padded (Prevents False Sharing)
struct GoodLayout {
    alignas(64) atomic<long long> count1{0};
    alignas(64) atomic<long long> count2{0}; 
};

struct OptimizedLayout {
    // This ensures the two variables are far enough apart 
    // to never sit on the same cache line on THIS specific CPU.
    alignas(std::hardware_destructive_interference_size) std::atomic<long long> count1{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<long long> count2{0};
};
void benchmark_false_sharing() {
    BadLayout bad;
    auto start = chrono::high_resolution_clock::now();
    
    jthread t1([&] { for(int i=0; i<10000000; ++i) bad.count1.fetch_add(1, memory_order_relaxed); });
    jthread t2([&] { for(int i=0; i<10000000; ++i) bad.count2.fetch_add(1, memory_order_relaxed); });
    
    t1.join(); t2.join();
    auto end = chrono::high_resolution_clock::now();
    cout << "Bad Layout (False Sharing): " << chrono::duration_cast<chrono::milliseconds>(end-start).count() << "ms\n";

    GoodLayout good;
    start = chrono::high_resolution_clock::now();
    
    jthread t3([&] { for(int i=0; i<10000000; ++i) good.count1.fetch_add(1, memory_order_relaxed); });
    jthread t4([&] { for(int i=0; i<10000000; ++i) good.count2.fetch_add(1, memory_order_relaxed); });
    
    t3.join(); t4.join();
    end = chrono::high_resolution_clock::now();
    cout << "Good Layout (Padded): " << chrono::duration_cast<chrono::milliseconds>(end-start).count() << "ms\n";

    OptimizedLayout optimized;
    start = chrono::high_resolution_clock::now();
    
    jthread t5([&] { for(int i=0; i<10000000; ++i) optimized.count1.fetch_add(1, memory_order_relaxed); });
    jthread t6([&] { for(int i=0; i<10000000; ++i) optimized.count2.fetch_add(1, memory_order_relaxed); });
    
    t5.join(); t6.join();
    end = chrono::high_resolution_clock::now();
    cout << "Optimized Layout (Hardware Destructive Interference Size): " << chrono::duration_cast<chrono::milliseconds>(end-start).count() << "ms\n";
}

int main() {
    benchmark_false_sharing();
    return 0;
}