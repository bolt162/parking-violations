#include <iostream>
#include <iomanip>
#include "parking.hpp"
#include "benchmark_harness.hpp"

using namespace parking;
using namespace parking::benchmark;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file> [iterations]" << std::endl;
        return 1;
    }

    std::string filepath = argv[1];
    int iterations = (argc >= 3) ? std::atoi(argv[2]) : 5;

    std::cout << "Phase 3 SoA Load Benchmark" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;

    std::vector<double> times;
    size_t record_count = 0;

    for (int i = 0; i < iterations; i++) {
        ParkingAPI engine;

        auto t0 = std::chrono::high_resolution_clock::now();
        record_count = engine.load(filepath);
        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times.push_back(ms);

        std::cout << "Run " << (i + 1) << ": " << std::fixed << std::setprecision(1)
                  << ms << " ms" << std::endl;
    }

    Stats stats = compute_stats(times);
    std::cout << "\nResults:" << std::endl;
    print_stats("Load time", stats);
    std::cout << "Records: " << record_count << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(0)
              << (record_count / (stats.mean / 1000.0)) << " records/sec" << std::endl;
    std::cout << "Peak RSS: " << get_peak_rss_mb() << " MB" << std::endl;

    return 0;
}
