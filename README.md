# C++ Counter Synchronization Demo

Last updated: March 19, 2026

This project demonstrates the effect of synchronization in multithreaded counter increments.

## What it compares

The program in `1.cpp` runs three scenarios with 4 threads, each doing 1,000,000 increments:

1. Without mutex (data race)
2. With mutex (`std::mutex` + `std::lock_guard`)
3. With atomic (`std::atomic<long long>`)

## Expected behavior

- Without mutex: final counter is usually incorrect due to race conditions.
- With mutex: final counter is correct (`4,000,000`) but slower due to lock contention.
- With atomic: final counter is correct (`4,000,000`) and typically faster than mutex in this test.

## Build and run

### Option 1: g++ (MinGW/WinLibs)

```bash
g++ -std=c++20 -O2 -pthread 1.cpp -o app.exe
app.exe
```

### Option 2: MSVC (Developer Command Prompt)

```bat
cl /std:c++20 /O2 /EHsc 1.cpp
1.exe
```

Note: The code redirects output to `output.txt` when `ONLINE_JUDGE` is not defined.

## Sample output

```text
Without mutex
Counter: 1492992
Time: 16474 us

With mutex
Counter: 4000000
Time: 201133 us

With atomic
Counter: 4000000
Time: 76565 us
```

## Files

- `1.cpp`: source code for the synchronization comparison.
- `output.txt`: captured program output.
