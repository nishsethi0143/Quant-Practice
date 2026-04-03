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

class AtomicDeltaTracker {
public:
    void apply_fill(long long signed_qty) {
        total_delta_.fetch_add(signed_qty, memory_order_relaxed);
    }

    long long current_delta() const {
        return total_delta_.load(memory_order_relaxed);
    }

    void reset() {
        total_delta_.store(0, memory_order_relaxed);
    }

private:
    atomic<long long> total_delta_{0};
};

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

void run_problem_103_delta_tracking() {
    constexpr int kMarketDataThreads = 4;
    constexpr int kFillsPerThread = 250000;

    AtomicDeltaTracker tracker;
    atomic<bool> done{false};
    atomic<long long> max_abs_delta{0};

    auto update_max_abs = [&](long long v) {
        long long abs_v = (v >= 0 ? v : -v);
        long long old = max_abs_delta.load(memory_order_relaxed);
        while (old < abs_v && !max_abs_delta.compare_exchange_weak(old, abs_v, memory_order_relaxed)) {
        }
    };

    auto start = chrono::high_resolution_clock::now();

    jthread detector([&] {
        while (!done.load(memory_order_relaxed)) {
            update_max_abs(tracker.current_delta());
        }
        update_max_abs(tracker.current_delta());
    });

    vector<jthread> feed_threads;
    feed_threads.reserve(kMarketDataThreads);

    for (int t = 0; t < kMarketDataThreads; ++t) {
        feed_threads.emplace_back([&, t] {
            for (int i = 0; i < kFillsPerThread; ++i) {
                const long long signed_fill = ((i + t) & 1) ? 1 : -1;
                tracker.apply_fill(signed_fill);
            }
        });
    }

    feed_threads.clear();
    done.store(true, memory_order_relaxed);
    detector.join();

    auto end = chrono::high_resolution_clock::now();
    auto elapsed_us = chrono::duration_cast<chrono::microseconds>(end - start).count();

    cout << "\nProblem 103 - Atomic Delta Tracking\n";
    cout << "Final Delta: " << tracker.current_delta() << "\n";
    cout << "Max |Delta| observed by detector: " << max_abs_delta.load(memory_order_relaxed) << "\n";
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
    run_problem_103_delta_tracking();
    return 0;
}