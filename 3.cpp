#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// ------------------------------------------------------------
// 1) Tagged pointer style stack (pointer/index + version in one CAS word)
// ------------------------------------------------------------
// To keep this demo portable, we store node index + version in one 64-bit word:
// [ high 32 bits: version | low 32 bits: node index ]
// This gives ABA protection similar to pointer+version double-width CAS.
class TaggedStack {
public:
	explicit TaggedStack(std::size_t capacity)
		: nodes_(capacity), next_free_(0) {
		for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(capacity); ++i) {
			nodes_[i].next_index = kNull;
		}
		head_.store(pack(kNull, 0), std::memory_order_relaxed);
	}

	bool push(int v) {
		std::uint32_t idx = next_free_.fetch_add(1, std::memory_order_relaxed);
		if (idx >= nodes_.size()) return false;

		nodes_[idx].value = v;

		while (true) {
			std::uint64_t old = head_.load(std::memory_order_acquire);
			std::uint32_t old_index = unpack_index(old);
			std::uint32_t old_ver = unpack_version(old);

			nodes_[idx].next_index = old_index;
			std::uint64_t desired = pack(idx, old_ver + 1);

			if (head_.compare_exchange_weak(
					old,
					desired,
					std::memory_order_release,
					std::memory_order_acquire)) {
				return true;
			}
		}
	}

	bool pop(int& out) {
		while (true) {
			std::uint64_t old = head_.load(std::memory_order_acquire);
			std::uint32_t old_index = unpack_index(old);
			std::uint32_t old_ver = unpack_version(old);

			if (old_index == kNull) return false;

			Node& n = nodes_[old_index];
			std::uint64_t desired = pack(n.next_index, old_ver + 1);

			if (head_.compare_exchange_weak(
					old,
					desired,
					std::memory_order_acq_rel,
					std::memory_order_acquire)) {
				out = n.value;
				return true;
			}
		}
	}

private:
	static constexpr std::uint32_t kNull = 0xFFFFFFFFu;

	struct Node {
		int value{0};
		std::uint32_t next_index{kNull};
	};

	static std::uint64_t pack(std::uint32_t index, std::uint32_t version) {
		return (static_cast<std::uint64_t>(version) << 32) | index;
	}

	static std::uint32_t unpack_index(std::uint64_t h) {
		return static_cast<std::uint32_t>(h & 0xFFFFFFFFu);
	}

	static std::uint32_t unpack_version(std::uint64_t h) {
		return static_cast<std::uint32_t>(h >> 32);
	}

	std::vector<Node> nodes_;
	std::atomic<std::uint32_t> next_free_;
	std::atomic<std::uint64_t> head_;
};

// ------------------------------------------------------------
// 2) Hazard pointer stack (safe memory reclamation)
// ------------------------------------------------------------
class HazardPointerStack {
public:
	HazardPointerStack() : head_(nullptr) {}

	~HazardPointerStack() {
		clear_all();
	}

	void push(int v) {
		Node* n = new Node{v, nullptr};
		Node* old = head_.load(std::memory_order_acquire);
		do {
			n->next = old;
		} while (!head_.compare_exchange_weak(
			old,
			n,
			std::memory_order_release,
			std::memory_order_acquire));
	}

	bool pop(int& out, std::size_t hp_slot) {
		while (true) {
			Node* old = head_.load(std::memory_order_acquire);
			if (!old) return false;

			// Publish the hazard so old cannot be reclaimed by another thread.
			set_hazard(hp_slot, old);

			// Re-check head after publishing hazard.
			if (head_.load(std::memory_order_acquire) != old) {
				clear_hazard(hp_slot);
				continue;
			}

			Node* next = old->next;
			if (head_.compare_exchange_weak(
					old,
					next,
					std::memory_order_acq_rel,
					std::memory_order_acquire)) {
				out = old->value;
				clear_hazard(hp_slot);
				retire_node(old);
				return true;
			}

			clear_hazard(hp_slot);
		}
	}

	void scan() {
		auto& retired = retired_local();
		std::vector<Node*> keep;
		keep.reserve(retired.size());

		for (Node* n : retired) {
			if (is_hazard(n)) {
				keep.push_back(n);
			} else {
				delete n;
			}
		}
		retired.swap(keep);
	}

private:
	struct Node {
		int value;
		Node* next;
	};

	static constexpr std::size_t kMaxHazardSlots = 128;
	static std::array<std::atomic<Node*>, kMaxHazardSlots>& hazards() {
		static std::array<std::atomic<Node*>, kMaxHazardSlots> hp{};
		return hp;
	}

	static void set_hazard(std::size_t slot, Node* p) {
		hazards()[slot].store(p, std::memory_order_release);
	}

	static void clear_hazard(std::size_t slot) {
		hazards()[slot].store(nullptr, std::memory_order_release);
	}

	static bool is_hazard(Node* p) {
		for (auto& h : hazards()) {
			if (h.load(std::memory_order_acquire) == p) return true;
		}
		return false;
	}

	static std::vector<Node*>& retired_local() {
		thread_local std::vector<Node*> retired;
		return retired;
	}

	static void retire_node(Node* p) {
		auto& retired = retired_local();
		retired.push_back(p);
		if (retired.size() >= 16) {
			// Lightweight periodic reclamation.
			std::vector<Node*> keep;
			keep.reserve(retired.size());
			for (Node* n : retired) {
				if (is_hazard(n)) keep.push_back(n);
				else delete n;
			}
			retired.swap(keep);
		}
	}

	void clear_all() {
		Node* n = head_.exchange(nullptr, std::memory_order_acq_rel);
		while (n) {
			Node* next = n->next;
			delete n;
			n = next;
		}
		scan();
	}

	std::atomic<Node*> head_;
};

struct BenchResult {
	double pop_ms{0.0};
	double push_ms{0.0};
	std::uint64_t pop_ops{0};
	std::uint64_t push_ops{0};
};

static BenchResult benchmark_tagged(std::size_t threads, std::size_t ops_per_thread) {
	const std::size_t total_ops = threads * ops_per_thread;
	TaggedStack stack(total_ops * 2 + 8);

	for (std::size_t i = 0; i < total_ops; ++i) {
		stack.push(static_cast<int>(i));
	}

	std::atomic<std::uint64_t> pop_index(0);
	auto pop_start = std::chrono::high_resolution_clock::now();
	std::vector<std::thread> workers;
	workers.reserve(threads);

	for (std::size_t t = 0; t < threads; ++t) {
		workers.emplace_back([&]() {
			int out = 0;
			while (true) {
				std::uint64_t ticket = pop_index.fetch_add(1, std::memory_order_relaxed);
				if (ticket >= total_ops) break;
				while (!stack.pop(out)) {
					std::this_thread::yield();
				}
			}
		});
	}

	for (auto& th : workers) th.join();
	auto pop_end = std::chrono::high_resolution_clock::now();

	std::atomic<std::uint64_t> push_index(0);
	auto push_start = std::chrono::high_resolution_clock::now();
	workers.clear();

	for (std::size_t t = 0; t < threads; ++t) {
		workers.emplace_back([&]() {
			while (true) {
				std::uint64_t ticket = push_index.fetch_add(1, std::memory_order_relaxed);
				if (ticket >= total_ops) break;
				while (!stack.push(static_cast<int>(ticket))) {
					std::this_thread::yield();
				}
			}
		});
	}

	for (auto& th : workers) th.join();
	auto push_end = std::chrono::high_resolution_clock::now();

	BenchResult r;
	r.pop_ms = std::chrono::duration<double, std::milli>(pop_end - pop_start).count();
	r.push_ms = std::chrono::duration<double, std::milli>(push_end - push_start).count();
	r.pop_ops = total_ops;
	r.push_ops = total_ops;
	return r;
}

static BenchResult benchmark_hazard(std::size_t threads, std::size_t ops_per_thread) {
	const std::size_t total_ops = threads * ops_per_thread;
	HazardPointerStack stack;

	for (std::size_t i = 0; i < total_ops; ++i) {
		stack.push(static_cast<int>(i));
	}

	std::atomic<std::uint64_t> pop_index(0);
	auto pop_start = std::chrono::high_resolution_clock::now();
	std::vector<std::thread> workers;
	workers.reserve(threads);

	for (std::size_t t = 0; t < threads; ++t) {
		workers.emplace_back([&, t]() {
			int out = 0;
			while (true) {
				std::uint64_t ticket = pop_index.fetch_add(1, std::memory_order_relaxed);
				if (ticket >= total_ops) break;
				while (!stack.pop(out, t)) {
					std::this_thread::yield();
				}
			}
			stack.scan();
		});
	}

	for (auto& th : workers) th.join();
	auto pop_end = std::chrono::high_resolution_clock::now();

	std::atomic<std::uint64_t> push_index(0);
	auto push_start = std::chrono::high_resolution_clock::now();
	workers.clear();

	for (std::size_t t = 0; t < threads; ++t) {
		workers.emplace_back([&]() {
			while (true) {
				std::uint64_t ticket = push_index.fetch_add(1, std::memory_order_relaxed);
				if (ticket >= total_ops) break;
				stack.push(static_cast<int>(ticket));
			}
		});
	}

	for (auto& th : workers) th.join();
	auto push_end = std::chrono::high_resolution_clock::now();

	BenchResult r;
	r.pop_ms = std::chrono::duration<double, std::milli>(pop_end - pop_start).count();
	r.push_ms = std::chrono::duration<double, std::milli>(push_end - push_start).count();
	r.pop_ops = total_ops;
	r.push_ops = total_ops;
	return r;
}

static double mops_per_sec(std::uint64_t ops, double ms) {
	if (ms <= 0.0) return 0.0;
	return static_cast<double>(ops) / (ms * 1000.0);
}

int main() {
	TaggedStack tagged(64);
	tagged.push(10);
	tagged.push(20);
	int a = 0;
	tagged.pop(a);

	HazardPointerStack hp;
	hp.push(100);
	hp.push(200);
	int b = 0;
	hp.pop(b, 0);
	hp.scan();
	hp.push(300); // Simulate a post-dividend price reset write.
	hp.scan();

	std::cout << "Tagged stack pop: " << a << '\n';
	std::cout << "Hazard stack pop: " << b << '\n';
	std::cout << "Dividend/reset-safe node update executed via hazard-pointer retire+scan.\n";

	std::cout << "\nComparison:\n";
	std::cout << "1) Problem 102 Arbitrage path: tagged CAS blocks ABA via versioned head updates.\n";
	std::cout << "2) Problem 105 Dividend resets: hazard pointers delay delete while readers may still observe old prices.\n";
	std::cout << "3) Production engines frequently combine both: versioned CAS + safe reclamation.\n";

	const std::size_t hw = std::thread::hardware_concurrency() == 0
		? 4
		: static_cast<std::size_t>(std::thread::hardware_concurrency());
	const std::size_t threads = (hw < 8 ? hw : 8);
	const std::size_t ops_per_thread = 200000;

	std::cout << "\nStress Test (" << threads << " threads, " << ops_per_thread
		<< " ops/thread per phase):\n";

	BenchResult tagged_bench = benchmark_tagged(threads, ops_per_thread);
	BenchResult hazard_bench = benchmark_hazard(threads, ops_per_thread);

	std::cout << "Tagged  pop: " << tagged_bench.pop_ms << " ms, "
		<< mops_per_sec(tagged_bench.pop_ops, tagged_bench.pop_ms) << " Mops/s\n";
	std::cout << "Tagged push: " << tagged_bench.push_ms << " ms, "
		<< mops_per_sec(tagged_bench.push_ops, tagged_bench.push_ms) << " Mops/s\n";

	std::cout << "Hazard  pop: " << hazard_bench.pop_ms << " ms, "
		<< mops_per_sec(hazard_bench.pop_ops, hazard_bench.pop_ms) << " Mops/s\n";
	std::cout << "Hazard push: " << hazard_bench.push_ms << " ms, "
		<< mops_per_sec(hazard_bench.push_ops, hazard_bench.push_ms) << " Mops/s\n";

	std::cout << "\nNote: hazard pointers add reclamation overhead on pop but guarantee safe node lifetime during cancels/resets.\n";

	return 0;
}
