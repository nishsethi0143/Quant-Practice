# Lock-Free Stack Learning Journey (from basics to HFT mindset)

Last updated: April 3, 2026

This guide explains the ideas behind `3.cpp` in depth so a new engineer can understand both correctness and performance trade-offs.

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

## Process timeline (day-by-day)

<a id="timeline-2026-03-25"></a>
### March 25, 2026 - lock-free foundation

1. Established the two core safety tracks in `3.cpp`:
- versioned tagged-head CAS for ABA resistance
- hazard-pointer retire/scan flow for memory safety
2. Added contention benchmark for push and pop throughput.
3. Focus:
- correctness mechanics first, performance interpretation second.

<a id="timeline-2026-04-03"></a>
### April 3, 2026 - production-facing interpretation pass

1. Kept algorithms stable and updated interpretation/output wording for finance use.
2. Mapped tagged CAS to Problem 102 arbitrage stale-state protection.
3. Mapped hazard-pointer reclamation to Problem 105 dividend/reset safety.
4. Focus:
- make reviewer understanding traceable from lock-free theory to market-engine behavior.

## Who this is for

- You know C++ basics and threads.
- You want to understand why lock-free code can still be wrong.
- You want practical intuition for ABA, tagged CAS, and hazard pointers.

## Why lock-free is hard

A lock-free stack seems simple:

1. Read `head`
2. Set `new_node->next = head`
3. CAS `head` from old to new

The trouble is that other threads can modify `head` between any two of your steps.

Two key risks:

1. ABA bug: head changes from A to B and back to A, tricking CAS.
2. Use-after-free: a thread still reads a node while another thread deletes it.

`3.cpp` addresses these with two different techniques.

## Strategy 1: Tagged pointer style CAS

### Core idea

Instead of comparing only node address/index, compare:

- pointer/index
- version tag

So the logical head value is a pair:

`(node, version)`

Each successful update increments version.

Even if node returns to the same address/index, version differs, so stale CAS fails.

### How it appears in `3.cpp`

In `TaggedStack`:

- Head is one atomic 64-bit word.
- Low 32 bits: node index.
- High 32 bits: version.

Helper functions:

- `pack(index, version)`
- `unpack_index(head_word)`
- `unpack_version(head_word)`

Push and pop both:

1. Load old packed head.
2. Build desired head with `version + 1`.
3. CAS old -> desired.

### Why this helps

It blocks classic ABA during CAS compare step.

### Limitation to remember

ABA protection does not solve memory reclamation by itself. If nodes are deleted and re-used unsafely, another thread can still dereference invalid memory.

In this demo, `TaggedStack` uses preallocated nodes and never frees them during runtime, so reclamation issues are avoided in a simple way.

## Strategy 2: Hazard pointers

### Core idea

A thread announces: "I am currently looking at node X."

That announcement is the hazard pointer.

Before deleting a removed node, we scan all hazard pointers:

- If any thread still hazards node X, do not delete yet.
- If nobody hazards node X, delete safely.

### How it appears in `3.cpp`

In `HazardPointerStack`:

- Global hazard slots: fixed array of atomic node pointers.
- `pop` uses slot `hp_slot` (one slot per thread in benchmark).

Pop sequence:

1. Load old head.
2. Publish hazard slot = old head.
3. Re-check head is still the same node.
4. CAS head to next.
5. Clear hazard.
6. Retire popped node.

Retire list logic:

- Nodes are not deleted immediately.
- They go into a thread-local retired vector.
- Periodic scan checks hazards and reclaims only safe nodes.

### Why this helps

It solves lifetime safety: a node is not freed while any reader may still touch it.

### Limitations to remember

- Hazard scanning adds overhead.
- You must size and manage hazard slots correctly.
- Production systems often need better batching and tuning than this demo.

## Memory ordering intuition

Both stacks use atomic operations with acquire/release style orderings.

Practical mental model:

- `release` on write/CAS publish: make prior writes visible before publication.
- `acquire` on read/load: once you observe published pointer, you also observe node contents initialized before publication.
- `acq_rel` on modifying CAS: combines both when replacing shared state.

If ordering is too weak, code may pass tests but fail under real contention and CPU reordering.

## Benchmark section in `3.cpp`

The stress test runs two phases for each stack:

1. Pop phase under contention.
2. Push phase under contention.

And reports:

- total milliseconds per phase
- throughput in Mops/s

### Why global ticket counters are used

Each worker takes work items using `fetch_add` ticketing.

This guarantees:

- no underflow
- exactly bounded total operations
- no accidental infinite loop due to counter wrap

## Reading benchmark results correctly

If tagged pop is faster, that is expected because:

- no retire/hazard scan overhead on pop

If hazard pop is slower, that is also expected because:

- extra hazard publication and safety checks
- deferred reclamation bookkeeping

Push comparison can vary because allocator behavior and thread scheduling matter.

### Important benchmark caveats

- This is throughput focused, not latency percentile focused.
- There is no thread affinity pinning.
- Warmup is minimal.
- Memory allocator effects are not isolated.

For HFT-like evaluation, you should also measure:

1. p50, p99, p99.9 operation latency
2. tail behavior during allocator pressure
3. behavior with mixed push/pop ratios
4. behavior with core pinning and NUMA awareness

## How to experiment

In `main` inside `3.cpp`, modify:

- `threads`
- `ops_per_thread`

Try:

1. low thread count (2)
2. medium (4)
3. high contention (8 or more, based on machine)

Then compare trend changes, not single-run absolute numbers.

## Production checklist (high level)

Before using ideas like this in a trading engine, validate:

1. Correctness under stress and sanitizer runs.
2. Clear ownership and lifetime rules for every node path.
3. Bounded memory growth under peak load.
4. Recovery behavior during thread stalls.
5. Tail latency, not only average throughput.

## Quick summary

- Tagged CAS protects against ABA at compare step.
- Hazard pointers protect node lifetime and reclamation safety.
- Real systems often combine both ideas.
- Correctness first, then optimize with measurement discipline.

## April 3, 2026 integration notes (Problems 102 and 105)

The current `3.cpp` output now explicitly maps lock-free techniques to the finance scenarios:

1. Problem 102 (Arbitrage): versioned CAS in `TaggedStack` protects against stale head observations (ABA) during rapid stack transitions.
2. Problem 105 (Dividends and resets): hazard-pointer retire/scan flow keeps reads safe while old nodes are being replaced or removed.

Operationally, this means strategy readers can continue traversing nodes while market-data logic performs updates and deferred reclamation without triggering use-after-free faults.
