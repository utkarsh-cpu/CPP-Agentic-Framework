#include <gtest/gtest.h>
#include "ConcurrencyPerformanceBenchmarks.cpp"

// Simple test runner for the concurrency benchmarks
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "Running Concurrency and Performance Benchmarks...\n";
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";
    std::cout << "SIMD support: " << (SIMDOps::is_simd_supported() ? "Yes" : "No") << "\n\n";
    
    return RUN_ALL_TESTS();
}