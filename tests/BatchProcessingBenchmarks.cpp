#include <benchmark/benchmark.h>
#include "../BaseNode/BatchData.h"
#include "../BaseNode/TypedData.h"
#include "../Flow/StringData.h"
#include <memory>
#include <string>
#include <vector>
#include <random>

// Global test data for benchmarks
class BenchmarkFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // Initialize test data pools
        setupTestData();
    }
    
    void TearDown(const ::benchmark::State& state) override {
        // Cleanup if needed
    }
    
protected:
    void setupTestData() {
        std::mt19937 rng(42); // Fixed seed for reproducible results
        
        // Create test data pools of different sizes
        small_pool_.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            small_pool_.push_back(std::make_shared<TypedData<int>>(rng()));
        }
        
        medium_pool_.reserve(100000);
        for (int i = 0; i < 100000; ++i) {
            medium_pool_.push_back(std::make_shared<TypedData<double>>(rng() * 0.001));
        }
        
        large_pool_.reserve(1000000);
        for (int i = 0; i < 1000000; ++i) {
            large_pool_.push_back(std::make_shared<StringData>("item_" + std::to_string(i)));
        }
    }
    
    std::vector<std::shared_ptr<TypedData<int>>> small_pool_;
    std::vector<std::shared_ptr<TypedData<double>>> medium_pool_;
    std::vector<std::shared_ptr<StringData>> large_pool_;
};

// Benchmark batch creation without reserve
BENCHMARK_DEFINE_F(BenchmarkFixture, BatchCreationNoReserve)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    
    for (auto _ : state) {
        BatchData batch;
        for (size_t i = 0; i < batch_size && i < small_pool_.size(); ++i) {
            batch.add_item(small_pool_[i]);
        }
        benchmark::DoNotOptimize(batch);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
    state.SetBytesProcessed(state.iterations() * batch_size * sizeof(std::shared_ptr<FrameworkData>));
}

// Benchmark batch creation with reserve
BENCHMARK_DEFINE_F(BenchmarkFixture, BatchCreationWithReserve)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    
    for (auto _ : state) {
        BatchData batch;
        batch.reserve(batch_size);
        for (size_t i = 0; i < batch_size && i < small_pool_.size(); ++i) {
            batch.add_item(small_pool_[i]);
        }
        benchmark::DoNotOptimize(batch);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
    state.SetBytesProcessed(state.iterations() * batch_size * sizeof(std::shared_ptr<FrameworkData>));
}

// Benchmark batch creation with constructor reserve
BENCHMARK_DEFINE_F(BenchmarkFixture, BatchCreationConstructorReserve)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    
    for (auto _ : state) {
        BatchData batch(batch_size);
        for (size_t i = 0; i < batch_size && i < small_pool_.size(); ++i) {
            batch.add_item(small_pool_[i]);
        }
        benchmark::DoNotOptimize(batch);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
    state.SetBytesProcessed(state.iterations() * batch_size * sizeof(std::shared_ptr<FrameworkData>));
}

// Benchmark range addition vs individual addition
BENCHMARK_DEFINE_F(BenchmarkFixture, BatchRangeAddition)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    
    // Prepare data for range addition
    std::vector<std::shared_ptr<FrameworkData>> items;
    items.reserve(batch_size);
    for (size_t i = 0; i < batch_size && i < small_pool_.size(); ++i) {
        items.push_back(small_pool_[i]);
    }
    
    for (auto _ : state) {
        BatchData batch;
        batch.reserve(batch_size);
        batch.add_range(items.begin(), items.end());
        benchmark::DoNotOptimize(batch);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
    state.SetBytesProcessed(state.iterations() * batch_size * sizeof(std::shared_ptr<FrameworkData>));
}

// Benchmark batch iteration performance
BENCHMARK_DEFINE_F(BenchmarkFixture, BatchIteration)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    
    // Setup batch
    BatchData batch(batch_size);
    for (size_t i = 0; i < batch_size && i < medium_pool_.size(); ++i) {
        batch.add_item(medium_pool_[i]);
    }
    
    for (auto _ : state) {
        double sum = 0.0;
        for (const auto& item : batch) {
            if (auto typed_item = item->template as<TypedData<double>>()) {
                sum += typed_item->get();
            }
        }
        benchmark::DoNotOptimize(sum);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}

// Benchmark type filtering performance
BENCHMARK_DEFINE_F(BenchmarkFixture, TypeFiltering)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    
    // Create mixed-type batch
    BatchData mixed_batch(batch_size);
    for (size_t i = 0; i < batch_size / 2 && i < small_pool_.size(); ++i) {
        mixed_batch.add_item(small_pool_[i]);
    }
    for (size_t i = 0; i < batch_size / 2 && i < medium_pool_.size(); ++i) {
        mixed_batch.add_item(medium_pool_[i]);
    }
    
    for (auto _ : state) {
        auto int_items = mixed_batch.filter_by_type<TypedData<int>>();
        benchmark::DoNotOptimize(int_items);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}

// Benchmark memory pool usage
BENCHMARK_DEFINE_F(BenchmarkFixture, MemoryPoolUsage)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    auto pool = std::make_shared<BatchMemoryPool>(1024 * 1024); // 1MB pool
    
    for (auto _ : state) {
        BatchData batch(pool, batch_size);
        for (size_t i = 0; i < batch_size && i < small_pool_.size(); ++i) {
            batch.add_item(small_pool_[i]);
        }
        benchmark::DoNotOptimize(batch);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}

// Benchmark batch copy operations
BENCHMARK_DEFINE_F(BenchmarkFixture, BatchCopy)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    
    // Setup source batch
    BatchData source_batch(batch_size);
    for (size_t i = 0; i < batch_size && i < small_pool_.size(); ++i) {
        source_batch.add_item(small_pool_[i]);
    }
    
    for (auto _ : state) {
        BatchData copied_batch(source_batch);
        benchmark::DoNotOptimize(copied_batch);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}

// Benchmark batch move operations
BENCHMARK_DEFINE_F(BenchmarkFixture, BatchMove)(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    
    for (auto _ : state) {
        // Create source batch
        BatchData source_batch(batch_size);
        for (size_t i = 0; i < batch_size && i < small_pool_.size(); ++i) {
            source_batch.add_item(small_pool_[i]);
        }
        
        // Move it
        BatchData moved_batch(std::move(source_batch));
        benchmark::DoNotOptimize(moved_batch);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}

// Register benchmarks with different sizes
BENCHMARK_REGISTER_F(BenchmarkFixture, BatchCreationNoReserve)
    ->Range(100, 100000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(BenchmarkFixture, BatchCreationWithReserve)
    ->Range(100, 100000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(BenchmarkFixture, BatchCreationConstructorReserve)
    ->Range(100, 100000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(BenchmarkFixture, BatchRangeAddition)
    ->Range(100, 100000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(BenchmarkFixture, BatchIteration)
    ->Range(1000, 1000000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(BenchmarkFixture, TypeFiltering)
    ->Range(1000, 100000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(BenchmarkFixture, MemoryPoolUsage)
    ->Range(100, 10000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(BenchmarkFixture, BatchCopy)
    ->Range(100, 10000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_REGISTER_F(BenchmarkFixture, BatchMove)
    ->Range(100, 10000)
    ->Unit(benchmark::kMicrosecond);

// Simple benchmarks for basic operations
static void BM_BatchDataConstruction(benchmark::State& state) {
    for (auto _ : state) {
        BatchData batch;
        benchmark::DoNotOptimize(batch);
    }
}
BENCHMARK(BM_BatchDataConstruction);

static void BM_BatchDataReserve(benchmark::State& state) {
    const size_t size = state.range(0);
    for (auto _ : state) {
        BatchData batch;
        batch.reserve(size);
        benchmark::DoNotOptimize(batch);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BatchDataReserve)->Range(8, 8<<20);

static void BM_TypedDataCreation(benchmark::State& state) {
    for (auto _ : state) {
        auto data = std::make_shared<TypedData<int>>(42);
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_TypedDataCreation);

static void BM_StringDataCreation(benchmark::State& state) {
    for (auto _ : state) {
        auto data = std::make_shared<StringData>("test_string");
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_StringDataCreation);

BENCHMARK_MAIN();