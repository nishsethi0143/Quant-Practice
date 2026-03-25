# Concurrency Learning Project (C++)

Last updated: March 25, 2026

This workspace contains multiple C++ concurrency examples, from beginner-safe synchronization to lock-free techniques used in low-latency systems.

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
	- False sharing benchmark:
	  - bad layout (two atomics likely sharing a cache line)
	  - padded 64-byte layout
	  - hardware-aware layout using `std::hardware_destructive_interference_size`
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
	- Deep explanation of false sharing, cache lines, and why padding improves throughput in `2.cpp`.
4. `journey.md`
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

1. `BadLayout` is usually the slowest because both counters can bounce the same cache line between cores.
2. `GoodLayout` and `OptimizedLayout` are usually faster because they reduce destructive interference.
3. Relative speedups vary per CPU, cache hierarchy, and OS scheduler behavior.

## Suggested study order

1. Run `1.cpp` and read `1.md`.
2. Run `2.cpp` and read `2.md`.
3. Run `3.cpp` and read `journey.md`.
4. Modify thread count and operations per thread in `2.cpp` and `3.cpp`, then re-run and compare trends.
