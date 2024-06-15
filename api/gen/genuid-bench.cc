#include <benchmark/benchmark.h>

#include "genuid.hpp"

static void BM_SomeFunction(benchmark::State &state) {
  genuid::InitParameters();
  for (auto _ : state) {
    std::size_t uid = genuid::GenerateUID();
  }
}

BENCHMARK(BM_SomeFunction);
BENCHMARK_MAIN();
