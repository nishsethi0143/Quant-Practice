# Concurrency Learning Project (C++)

Last updated: April 3, 2026

This workspace contains multiple C++ concurrency examples, from beginner-safe synchronization to lock-free techniques used in low-latency systems.

## Timeline cross-links

### March 25, 2026

1. [README.md](README.md#timeline-2026-03-25)
2. [1.md](1.md#timeline-2026-03-25)
3. [2.md](2.md#timeline-2026-03-25)
4. [3.md](3.md#timeline-2026-03-25)
5. [journey.md](journey.md#timeline-2026-03-25)

### April 3, 2026

1. [README.md](README.md#timeline-2026-04-03)
2. [1.md](1.md#timeline-2026-04-03)
3. [2.md](2.md#timeline-2026-04-03)
4. [3.md](3.md#timeline-2026-04-03)
5. [journey.md](journey.md#timeline-2026-04-03)

## Project history (process-by-process)

<a id="timeline-2026-03-25"></a>
### March 25, 2026 - baseline concurrency learning set

1. Built `1.cpp` as synchronization comparison:
	- no mutex (race demonstration)
	- mutex-protected counter
	- atomic counter benchmark
2. Built `2.cpp` baseline false-sharing benchmark using earlier naming (`BadLayout`, `GoodLayout`, `OptimizedLayout`).
3. Built `3.cpp` lock-free stack demo with tagged CAS and hazard pointers.
4. Added learning-first long-form documentation (`1.md`, `2.md`, `journey.md`).

<a id="timeline-2026-04-03"></a>
### April 3, 2026 - finance integration pass

1. Problem 103:
	- integrated atomic delta tracking in `1.cpp` with relaxed atomics and detector thread.
2. Problems 102 and 104:
	- refactored `2.cpp` into call/put feed layout benchmark (`SharedLineBook`, `CacheAlignedBook64`, `CacheAlignedBookHw`).
3. Problems 105 and 102:
	- retained `3.cpp` core algorithms and updated output/narrative mapping for dividend-reset safety and arbitrage-safe ABA handling.
4. Documentation sync:
	- refreshed all markdown files and added `3.md` to preserve earlier-vs-new interpretation in one place.

## Project goals

1. Show what goes wrong with unsynchronized shared state.
2. Compare mutex and atomic synchronization for correctness and speed.
3. Introduce ABA protection and safe memory reclamation in lock-free structures.
4. Provide runnable stress tests and narrative documentation for learning.

## Code map

1. `1.cpp`
	- Counter increment comparison:
	  - no mutex (data race)
	  - mutex (`std::mutex` + `std::lock_guard`)
	  - atomic (`std::atomic<long long>`)
2. `2.cpp`
	- False sharing benchmark for call/put feeds:
	  - `SharedLineBook`
	  - `CacheAlignedBook64`
	  - `CacheAlignedBookHw` using `std::hardware_destructive_interference_size`
3. `3.cpp`
	- Lock-free stack comparison:
	  - tagged pointer style CAS (index + version packed into one atomic word)
	  - hazard pointer style deferred reclamation
	- Includes a multithreaded stress test with throughput reporting.

## Documentation map

1. `README.md`
	- Project-level overview and run instructions.
2. `1.md`
	- Deep line-by-line walkthrough of `1.cpp`.
3. `2.md`
	- Deep explanation of false sharing in call/put feeds and cache-line aware layout in `2.cpp`.
4. `3.md`
	- Side-by-side note of the earlier `3.cpp` interpretation and the new finance-integrated interpretation.
5. `journey.md`
	- Detailed, end-to-end explanation of lock-free stack design in `3.cpp`, including ABA, hazard pointers, memory ordering, and benchmark interpretation.

## Build and run

### g++ (MinGW/WinLibs)

```bash
g++ -std=c++20 -O2 -pthread 1.cpp -o 1.exe
./1.exe

g++ -std=c++20 -O2 -pthread 2.cpp -o 2.exe
./2.exe

g++ -std=c++20 -O2 -pthread 3.cpp -o 3.exe
./3.exe
```

### MSVC (Developer Command Prompt)

```bat
cl /std:c++20 /O2 /EHsc 1.cpp
1.exe

cl /std:c++20 /O2 /EHsc 2.cpp
2.exe

cl /std:c++20 /O2 /EHsc 3.cpp
3.exe
```

## What to expect from 3.cpp

1. Tagged-pointer stack typically shows strong pop throughput due to simple node storage and no dynamic allocation on pop.
2. Hazard-pointer stack pays extra pop overhead for safe reclamation checks and retire-scan logic.
3. Push performance can vary by machine and allocator behavior.

Important: The current `3.cpp` is a learning demo, not production-ready HFT infrastructure.

## What to expect from 2.cpp

1. `SharedLineBook` is usually the slowest because call/put feeds can bounce the same cache line between cores.
2. `CacheAlignedBook64` and `CacheAlignedBookHw` are usually faster because they reduce destructive interference.
3. Relative speedups vary per CPU, cache hierarchy, and OS scheduler behavior.

## Finance integration (Problems 102-105)

1. `1.cpp` implements atomic risk tracking for Problem 103 (Delta):
	- Uses `std::atomic<long long>` with `memory_order_relaxed` for high-frequency signed fill updates.
	- Includes a detector thread that continuously reads delta without mutex blocking.
2. `2.cpp` implements cache-aligned call/put feed layout for Problems 102 and 104:
	- Compares a shared-line book against `alignas(64)` and hardware-aware alignment.
	- Demonstrates lower core contention when call/put updates are split across cache lines.
3. `3.cpp` implements safe memory reclamation for Problems 105 and 102:
	- Uses hazard pointers to prevent use-after-free during cancels/resets.
	- Uses versioned CAS (tagged head) to reduce ABA risk in lock-free updates.

| Component | Source | Finance application |
| --- | --- | --- |
| Lock-Free Counters | `1.cpp` | Real-time delta and position tracking |
| Cache Padding | `2.cpp` | Separate call/put feeds to reduce destructive interference |
| Hazard Pointers | `3.cpp` | Safe reads while market-data thread updates/cancels nodes |
| ABA Protection | `3.cpp` | Versioned CAS to prevent stale execution paths |

## Suggested study order

1. Run `1.cpp` and read `1.md`.
2. Run `2.cpp` and read `2.md`.
3. Read `3.md` for earlier-vs-new `3.cpp` mapping.
4. Run `3.cpp` and read `journey.md`.
5. Modify thread count and operations per thread in `2.cpp` and `3.cpp`, then re-run and compare trends.
