#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <omp.h>

#include "parking.hpp"
#include "record.hpp"
#include "benchmark_harness.hpp"

using namespace parking;

// Parse a comma-separated list of integers, e.g. "1,2,4,8,16"
static std::vector<int> parse_int_list(const std::string& s) {
    std::vector<int> result;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t comma = s.find(',', pos);
        if (comma == std::string::npos) comma = s.size();
        int val = std::atoi(s.substr(pos, comma - pos).c_str());
        if (val > 0) result.push_back(val);
        pos = comma + 1;
    }
    return result;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <csv_file> [iterations] [output.csv] [--indexed] "
                  << "[--threads=N] [--thread-scaling=1,2,4,8]"
                  << std::endl;
        std::cerr << "  iterations:          timed runs per query (default: 12)"
                  << std::endl;
        std::cerr << "  output.csv:          write results to CSV file" << std::endl;
        std::cerr << "  --indexed:           use IndexedSearch" << std::endl;
        std::cerr << "  --threads=N:         set OpenMP thread count (default: 6)" << std::endl;
        std::cerr << "  --thread-scaling=..: load once, run at each thread count"
                  << std::endl;
        return 1;
    }

    // Default to 6 threads (optimal for search per benchmarks)
    omp_set_num_threads(6);

    const std::string filepath = argv[1];
    int iterations = 12;
    std::string csv_out;
    bool use_indexed = false;
    std::vector<int> scaling_threads;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--indexed") {
            use_indexed = true;
        } else if (arg.rfind("--threads=", 0) == 0) {
            int t = std::atoi(arg.c_str() + 10);
            if (t > 0) omp_set_num_threads(t);
        } else if (arg.rfind("--thread-scaling=", 0) == 0) {
            scaling_threads = parse_int_list(arg.substr(17));
        } else if (csv_out.empty() && arg.find(".csv") != std::string::npos) {
            csv_out = arg;
        } else {
            int val = std::atoi(argv[i]);
            if (val > 0) iterations = val;
        }
    }

    const char* engine_name = use_indexed ? "IndexedSearch" : "ParallelSearch";

    std::cout << "============================================" << std::endl;
    std::cout << "  Search Benchmark" << std::endl;
    std::cout << "  File: " << filepath << std::endl;
    std::cout << "  Engine: " << engine_name << std::endl;
    if (!scaling_threads.empty()) {
        std::cout << "  Thread scaling mode: ";
        for (size_t i = 0; i < scaling_threads.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << scaling_threads[i];
        }
        std::cout << std::endl;
    } else {
        std::cout << "  Threads: " << omp_get_max_threads() << std::endl;
    }
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
    auto run_search = [&](const std::string& id, auto query_fn,
                          int thread_count) {
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
        entry.engine = std::string(engine_name) + "_t" + std::to_string(thread_count);
        entry.time_stats = stats;
        entry.records_scanned = scanned;
        entry.records_matched = matched;
        entry.throughput = throughput;
        results.push_back(entry);
    };

    // Helper: benchmark an aggregation query
    auto run_agg = [&](const std::string& id, auto query_fn,
                       int thread_count) {
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
        entry.engine = std::string(engine_name) + "_t" + std::to_string(thread_count);
        entry.time_stats = stats;
        entry.records_scanned = scanned;
        entry.records_matched = matched;
        entry.throughput = throughput;
        results.push_back(entry);
    };

    // Helper: benchmark a SIMD-friendly query (count only)
    auto run_simd_count = [&](const std::string& id, auto query_fn, int thread_count) {
        std::cout << "-- " << id << " (SIMD-friendly) --" << std::endl;

        auto warmup = query_fn();
        size_t scanned = warmup.total_scanned;
        size_t matched = warmup.count;

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
                  << "  Count: " << matched
                  << "  Throughput: " << std::fixed << std::setprecision(0)
                  << throughput << " rec/s\n" << std::endl;

        benchmark::BenchmarkEntry entry;
        entry.name = id;
        entry.engine = std::string(engine_name) + "_t" + std::to_string(thread_count);
        entry.time_stats = stats;
        entry.records_scanned = scanned;
        entry.records_matched = matched;
        entry.throughput = throughput;
        results.push_back(entry);
    };

    // Helper: benchmark a SIMD-friendly minmax query
    auto run_simd_minmax = [&](const std::string& id, auto query_fn, int thread_count) {
        std::cout << "-- " << id << " (SIMD-friendly) --" << std::endl;

        auto warmup = query_fn();
        size_t scanned = warmup.total_scanned;

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
                  << "  min=" << warmup.min_date << ", max=" << warmup.max_date
                  << "  Throughput: " << std::fixed << std::setprecision(0)
                  << throughput << " rec/s\n" << std::endl;

        benchmark::BenchmarkEntry entry;
        entry.name = id;
        entry.engine = std::string(engine_name) + "_t" + std::to_string(thread_count);
        entry.time_stats = stats;
        entry.records_scanned = scanned;
        entry.records_matched = 2;
        entry.throughput = throughput;
        results.push_back(entry);
    };

    // Helper: run all benchmarks at a given thread count
    auto run_all = [&](int thread_count) {
        // SIMD-friendly queries first
        run_simd_count("simd_count", [&]() { return engine.count_in_date_range(20240101, 20241231); }, thread_count);
        run_simd_minmax("simd_minmax", [&]() { return engine.find_date_extremes(); }, thread_count);

        run_search("B1_date_narrow", [&]() { return engine.find_by_date_range(20240101, 20240131); }, thread_count);
        run_search("B2_date_wide",   [&]() { return engine.find_by_date_range(20240101, 20241231); }, thread_count);
        run_search("B3_code_common", [&]() { return engine.find_by_violation_code(46); }, thread_count);
        run_search("B4_code_rare",   [&]() { return engine.find_by_violation_code(99); }, thread_count);
        run_search("B5_plate",       [&]() { return engine.find_by_plate(sample_plate.c_str()); }, thread_count);
        run_search("B6_state_common",[&]() { return engine.find_by_state(ny_enum); }, thread_count);
        run_search("B7_state_rare",  [&]() { return engine.find_by_state(fl_enum); }, thread_count);
        run_search("B8_county",      [&]() { return engine.find_by_county(bk_enum); }, thread_count);
        run_agg("B9_precinct_agg",   [&]() { return engine.count_by_precinct(); }, thread_count);
        run_agg("B10_fy_agg",        [&]() { return engine.count_by_fiscal_year(); }, thread_count);
    };

    // -- Thread-scaling mode --

    if (!scaling_threads.empty()) {
        for (int t : scaling_threads) {
            omp_set_num_threads(t);
            std::cout << "\n############################################" << std::endl;
            std::cout << "  THREAD COUNT: " << t << std::endl;
            std::cout << "############################################\n" << std::endl;

            size_t before = results.size();
            run_all(t);

            // Print summary for this thread count
            std::cout << "============================================" << std::endl;
            std::cout << "  Summary (" << engine_name << " t=" << t << ")"
                      << std::endl;
            std::cout << "============================================" << std::endl;

            for (size_t i = before; i < results.size(); ++i) {
                const auto& r = results[i];
                std::cout << "  " << std::left << std::setw(20) << r.name
                          << std::right << std::fixed << std::setprecision(2)
                          << "  " << std::setw(10) << r.time_stats.mean << " ms"
                          << "  matched=" << std::setw(10) << r.records_matched
                          << "  throughput=" << std::setprecision(0) << std::setw(14)
                          << r.throughput << " rec/s"
                          << std::endl;
            }

            // Write per-thread-count CSV
            if (!csv_out.empty()) {
                std::string per_csv = csv_out;
                size_t dot = per_csv.rfind(".csv");
                if (dot != std::string::npos) {
                    per_csv = per_csv.substr(0, dot) + "_t"
                            + std::to_string(t) + ".csv";
                } else {
                    per_csv += "_t" + std::to_string(t) + ".csv";
                }
                std::ofstream ofs(per_csv);
                benchmark::print_csv_header(ofs);
                for (size_t i = before; i < results.size(); ++i) {
                    benchmark::print_csv_row(results[i], ofs);
                }
                std::cout << "  Results written to: " << per_csv << std::endl;
            }
        }

        std::cout << "\n  Peak RSS: " << benchmark::get_peak_rss_mb() << " MB"
                  << std::endl;
        std::cout << "\nThread scaling study complete." << std::endl;
        return 0;
    }

    // -- Single-run mode --

    int num_threads = omp_get_max_threads();
    run_all(num_threads);

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
