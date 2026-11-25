#include "jianhuan/engine.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

long long micros(Clock::time_point start, Clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

void percentile(std::vector<long long>& values, double p, long long& out) {
  if (values.empty()) {
    out = 0;
    return;
  }
  std::sort(values.begin(), values.end());
  const std::size_t index = static_cast<std::size_t>(p * (values.size() - 1));
  out = values[index];
}

void print_memory(const char* stage) {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS counters = {};
  if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
    std::cout << stage << "_working_set_mb=" << counters.WorkingSetSize / (1024.0 * 1024.0) << "\n";
    std::cout << stage << "_private_mb=" << counters.PagefileUsage / (1024.0 * 1024.0) << "\n";
  }
#endif
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: jianhuan_bench <shared_data_dir> <user_data_dir> [schema_id]\n";
    return 1;
  }

  auto& engine = jianhuan::Engine::instance();
  const auto init_start = Clock::now();
  if (!engine.initialize(argv[1], argv[2])) {
    std::cerr << "initialize failed\n";
    return 1;
  }
  const auto init_end = Clock::now();
  print_memory("after_init");

  const std::uint64_t session = engine.create_session();
  if (session == 0) {
    std::cerr << "create_session failed\n";
    return 1;
  }
  const std::string schema = argc >= 4 ? argv[3] : "jianhuan_pinyin";
  engine.select_schema(session, schema);

  const std::string sequence = "women";
  for (char c : sequence) {
    engine.process_key(session, static_cast<unsigned char>(c), 0);
  }
  engine.clear_composition(session);

  constexpr int kIterations = 500;
  std::vector<long long> key_latencies;
  key_latencies.reserve(kIterations * sequence.size());

  const auto loop_start = Clock::now();
  for (int i = 0; i < kIterations; ++i) {
    for (char c : sequence) {
      const auto start = Clock::now();
      engine.process_key(session, static_cast<unsigned char>(c), 0);
      jianhuan::Context context;
      engine.get_context(session, context);
      const auto end = Clock::now();
      key_latencies.push_back(micros(start, end));
    }
    engine.clear_composition(session);
  }
  const auto loop_end = Clock::now();

  long long p50 = 0;
  long long p95 = 0;
  long long p99 = 0;
  percentile(key_latencies, 0.50, p50);
  percentile(key_latencies, 0.95, p95);
  percentile(key_latencies, 0.99, p99);
  const long long max = *std::max_element(key_latencies.begin(), key_latencies.end());
  const long long total = micros(loop_start, loop_end);

  std::cout << "schema=" << schema << "\n";
  std::cout << "initialize_ms=" << micros(init_start, init_end) / 1000.0 << "\n";
  std::cout << "keys=" << key_latencies.size() << "\n";
  std::cout << "key_p50_us=" << p50 << "\n";
  std::cout << "key_p95_us=" << p95 << "\n";
  std::cout << "key_p99_us=" << p99 << "\n";
  std::cout << "key_max_us=" << max << "\n";
  std::cout << "loop_ms=" << total / 1000.0 << "\n";
  print_memory("after_loop");

  engine.destroy_session(session);
  engine.shutdown();
  return 0;
}