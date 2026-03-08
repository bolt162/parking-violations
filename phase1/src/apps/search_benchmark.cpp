#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>

#include "parking.hpp"
#include "record.hpp"
#include "benchmark_harness.hpp"

using namespace parking;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <csv_file> [iterations] [output.csv] [--indexed]"
                  << std::endl;
        return 1;
    }

    const std::string filepath = argv[1];
    int iterations = 12;
    std::string csv_out;
    bool use_indexed = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--indexed") {
            use_indexed = true;
        } else if (csv_out.empty() && arg.find(".csv") != std::string::npos) {
            csv_out = arg;
        } else {
            int val = std::atoi(argv[i]);
            if (val > 0) iterations = val;
        }
    }

    const char* engine_name = use_indexed ? "IndexedSearch" : "LinearSearch";

    std::cout << "============================================" << std::endl;
    std::cout << "  Search Benchmark" << std::endl;
    std::cout << "  File: " << filepath << std::endl;
    std::cout << "  Engine: " << engine_name << std::endl;
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "============================================\n" << std::endl;

    // Load data once
    ParkingAPI engine(use_indexed);

    std::cout << "Loading data..." << std::endl;
    auto tl0 = std::chrono::high_resolution_clock::now();
    size_t count = engine.load(filepath);
    auto tl1 = std::chrono::high_resolution_clock::now();
    double load_s = std::chrono::duration<double>(tl1 - tl0).count();

    std::cout << "  Loaded " << count << " records in "
              << std::fixed << std::setprecision(1) << load_s << "s"
              << std::endl;
    std::cout << "  Peak RSS: " << benchmark::get_peak_rss_mb() << " MB\n"
              << std::endl;

    // Grab sample plate from first record
    const auto& r0 = engine.record(0);
    const auto& pool = engine.text_pool();
    uint8_t plate_len = r0.str_lengths[SF_PLATE_ID];
    std::string sample_plate(pool.get(r0.str_offsets[SF_PLATE_ID]), plate_len);

    uint8_t ny_enum = state_to_enum("NY", 2);
    uint8_t fl_enum = state_to_enum("FL", 2);
    uint8_t bk_enum = county_to_enum("BK", 2);

    std::vector<benchmark::BenchmarkEntry> results;

    // Helper: benchmark a search query
    auto run_search = [&](const std::string& id, auto query_fn) {
        std::cout << "-- " << id << " --" << std::endl;

        // warmup
        auto warmup = query_fn();
        size_t scanned = warmup.total_scanned;
        size_t matched = warmup.count();

        std::vector<double> times;
        times.reserve(iterations);
        for (int i = 0; i < iterations; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            auto res = query_fn();
            auto t1 = std::chrono::high_resolution_clock::now();
            times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        auto stats = benchmark::compute_stats(times);
        double throughput = scanned / (stats.mean / 1000.0);
        benchmark::print_stats(id, stats);
        std::cout << "    Scanned: " << scanned
                  << "  Matched: " << matched
                  << "  Throughput: " << std::fixed << std::setprecision(0)
                  << throughput << " rec/s\n" << std::endl;

        benchmark::BenchmarkEntry entry;
        entry.name = id;
        entry.engine = engine_name;
        entry.time_stats = stats;
        entry.records_scanned = scanned;
        entry.records_matched = matched;
        entry.throughput = throughput;
        results.push_back(entry);
    };

    // Helper: benchmark an aggregation query
    auto run_agg = [&](const std::string& id, auto query_fn) {
        std::cout << "-- " << id << " --" << std::endl;

        auto warmup = query_fn();
        size_t scanned = warmup.total_scanned;
        size_t matched = warmup.counts.size();

        std::vector<double> times;
        times.reserve(iterations);
        for (int i = 0; i < iterations; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            auto res = query_fn();
            auto t1 = std::chrono::high_resolution_clock::now();
            times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        auto stats = benchmark::compute_stats(times);
        double throughput = scanned / (stats.mean / 1000.0);
        benchmark::print_stats(id, stats);
        std::cout << "    Scanned: " << scanned
                  << "  Categories: " << matched
                  << "  Throughput: " << std::fixed << std::setprecision(0)
                  << throughput << " rec/s\n" << std::endl;

        benchmark::BenchmarkEntry entry;
        entry.name = id;
        entry.engine = engine_name;
        entry.time_stats = stats;
        entry.records_scanned = scanned;
        entry.records_matched = matched;
        entry.throughput = throughput;
        results.push_back(entry);
    };

    // Run all benchmarks
    run_search("B1_date_narrow", [&]() { return engine.find_by_date_range(20240101, 20240131); });
    run_search("B2_date_wide",   [&]() { return engine.find_by_date_range(20240101, 20241231); });
    run_search("B3_code_common", [&]() { return engine.find_by_violation_code(46); });
    run_search("B4_code_rare",   [&]() { return engine.find_by_violation_code(99); });
    run_search("B5_plate",       [&]() { return engine.find_by_plate(sample_plate.c_str()); });
    run_search("B6_state_common",[&]() { return engine.find_by_state(ny_enum); });
    run_search("B7_state_rare",  [&]() { return engine.find_by_state(fl_enum); });
    run_search("B8_county",      [&]() { return engine.find_by_county(bk_enum); });
    run_agg("B9_precinct_agg",   [&]() { return engine.count_by_precinct(); });
    run_agg("B10_fy_agg",        [&]() { return engine.count_by_fiscal_year(); });

    // Summary
    std::cout << "============================================" << std::endl;
    std::cout << "  Summary (" << engine_name << ")" << std::endl;
    std::cout << "============================================" << std::endl;

    for (const auto& r : results) {
        std::cout << "  " << std::left << std::setw(20) << r.name
                  << std::right << std::fixed << std::setprecision(2)
                  << "  " << std::setw(10) << r.time_stats.mean << " ms"
                  << "  matched=" << std::setw(10) << r.records_matched
                  << "  throughput=" << std::setprecision(0) << std::setw(14)
                  << r.throughput << " rec/s"
                  << std::endl;
    }

    std::cout << "\n  Peak RSS: " << benchmark::get_peak_rss_mb() << " MB"
              << std::endl;

    // CSV output
    if (!csv_out.empty()) {
        std::ofstream ofs(csv_out);
        benchmark::print_csv_header(ofs);
        for (const auto& r : results) {
            benchmark::print_csv_row(r, ofs);
        }
        std::cout << "\n  Results written to: " << csv_out << std::endl;
    }

    std::cout << "\nDone." << std::endl;
    return 0;
}
