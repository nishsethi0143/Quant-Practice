#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using namespace std;

long long counter = 0;
atomic<long long> atomic_counter = 0;
mutex counter_mutex;

constexpr int kThreads = 4;
constexpr int kIncrementsPerThread = 1000000;

void run_without_mutex(){
    counter = 0;
    auto start = chrono::high_resolution_clock::now();
    vector<jthread> workers;
    workers.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([] {
            for (int i = 0; i < kIncrementsPerThread; ++i) {
                ++counter;
            }
        });
    }

    workers.clear(); // joins all jthreads now
    auto end = chrono::high_resolution_clock::now();

    auto elapsed_us = chrono::duration_cast<chrono::microseconds>(end - start).count();
    cout << "Without mutex\n";
    cout << "Counter: " << counter << "\n";
    cout << "Time: " << elapsed_us << " us\n\n";
}

void run_with_mutex() {
    counter = 0;

    auto start = chrono::high_resolution_clock::now();
    vector<jthread> workers;
    workers.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([] {
            for (int i = 0; i < kIncrementsPerThread; ++i) {
                lock_guard<mutex> lock(counter_mutex);
                ++counter;
            }
        });
    }

    workers.clear(); // joins all jthreads now
    auto end = chrono::high_resolution_clock::now();

    auto elapsed_us = chrono::duration_cast<chrono::microseconds>(end - start).count();
    cout << "With mutex\n";
    cout << "Counter: " << counter << "\n";
    cout << "Time: " << elapsed_us << " us\n\n";
}

void run_with_atomic() {
    atomic_counter.store(0, memory_order_relaxed);

    auto start = chrono::high_resolution_clock::now();
    vector<jthread> workers;
    workers.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([] {
            for (int i = 0; i < kIncrementsPerThread; ++i) {
                atomic_counter.fetch_add(1, memory_order_relaxed);
            }
        });
    }

    workers.clear(); // joins all jthreads now
    auto end = chrono::high_resolution_clock::now();

    auto elapsed_us = chrono::duration_cast<chrono::microseconds>(end - start).count();
    cout << "With atomic\n";
    cout << "Counter: " << atomic_counter.load(memory_order_relaxed) << "\n";
    cout << "Time: " << elapsed_us << " us\n";
}

int32_t main() {
    #ifndef ONLINE_JUDGE
    freopen("input.txt", "r", stdin);
    freopen("output.txt", "w", stdout);
    #endif
    run_without_mutex();
    run_with_mutex();
    run_with_atomic();
    return 0;
}