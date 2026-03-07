#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

#include "api/parking_api.hpp"
#include "core/violation_record.hpp"
#include "benchmark_harness.hpp"

using namespace parking;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <csv_file> [iterations] [output.csv]" << std::endl;
        std::cerr << "  iterations: number of timed runs (default: 12)"
                  << std::endl;
        std::cerr << "  output.csv: write results to CSV file" << std::endl;
        return 1;
    }

    const std::string filepath = argv[1];
    const int iterations = (argc >= 3) ? std::atoi(argv[2]) : 12;
    const std::string csv_out = (argc >= 4) ? argv[3] : "";

    std::cout << "============================================" << std::endl;
    std::cout << "  Load Benchmark" << std::endl;
    std::cout << "  File: " << filepath << std::endl;
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  sizeof(ViolationRecord) = "
              << sizeof(core::ViolationRecord) << " bytes" << std::endl;
    std::cout << "============================================\n" << std::endl;

    if (iterations < 1) {
        std::cerr << "Iterations must be >= 1" << std::endl;
        return 1;
    }

    // ── Benchmark: Full load (read + parse + populate) ───────────────────

    std::vector<double> load_times;
    load_times.reserve(iterations);

    size_t record_count = 0;

    // Warmup run (also determines record count)
    {
        std::cout << "Warmup run..." << std::endl;
        api::ParkingAPI engine;
        record_count = engine.load(filepath);
        std::cout << "  Records: " << record_count << std::endl;
        std::cout << "  Peak RSS after warmup: "
                  << benchmark::get_peak_rss_mb() << " MB\n" << std::endl;
    }

    // Timed runs
    std::cout << "Running " << iterations << " timed iterations..." << std::endl;

    for (int i = 0; i < iterations; ++i) {
        api::ParkingAPI engine;

        auto t0 = std::chrono::high_resolution_clock::now();
        engine.load(filepath);
        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        load_times.push_back(ms);

        double throughput = record_count / (ms / 1000.0);
        std::cout << "  Run " << (i + 1) << "/" << iterations
                  << ": " << std::fixed << std::setprecision(1) << ms << " ms"
                  << " (" << std::setprecision(0) << throughput << " rec/s)"
                  << std::endl;
    }

    // ── Results ──────────────────────────────────────────────────────────

    std::cout << std::endl;
    auto stats = benchmark::compute_stats(load_times);

    std::cout << "── Load Benchmark Results ──" << std::endl;
    benchmark::print_stats("Full load", stats);
    std::cout << std::endl;

    double mean_throughput = record_count / (stats.mean / 1000.0);
    double mem_gb = (record_count * sizeof(core::ViolationRecord))
                    / (1024.0 * 1024.0 * 1024.0);

    std::cout << "  Records:           " << record_count << std::endl;
    std::cout << "  Struct size:       " << sizeof(core::ViolationRecord) << " bytes" << std::endl;
    std::cout << "  Mean throughput:   " << std::fixed << std::setprecision(0)
              << mean_throughput << " records/sec" << std::endl;
    std::cout << "  In-memory structs: " << std::setprecision(2)
              << mem_gb << " GB" << std::endl;
    std::cout << "  Peak RSS:          " << std::setprecision(1)
              << benchmark::get_peak_rss_mb() << " MB" << std::endl;

    // ── CSV output ───────────────────────────────────────────────────────

    if (!csv_out.empty()) {
        std::ofstream ofs(csv_out);
        benchmark::print_csv_header(ofs);

        benchmark::BenchmarkEntry entry;
        entry.name = "full_load";
        entry.engine = "serial";
        entry.time_stats = stats;
        entry.records_scanned = record_count;
        entry.records_matched = record_count;
        entry.throughput = mean_throughput;
        benchmark::print_csv_row(entry, ofs);

        std::cout << "\n  Results written to: " << csv_out << std::endl;
    }

    std::cout << "\nDone." << std::endl;
    return 0;
}
