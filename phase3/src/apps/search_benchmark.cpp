#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <omp.h>
#include "parking.hpp"
#include "record.hpp"
#include "benchmark_harness.hpp"

using namespace parking;
using namespace parking::benchmark;

// Parse comma-separated list of integers
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
        std::cerr << "Usage: " << argv[0] << " <csv_file> [iterations] [output.csv] "
                  << "[--threads=N] [--thread-scaling=1,2,4,8]" << std::endl;
        return 1;
    }

    std::string filepath = argv[1];
    int iterations = 12;
    std::string csv_out;
    std::vector<int> scaling_threads;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--threads=", 0) == 0) {
            int t = std::atoi(arg.c_str() + 10);
            if (t > 0) omp_set_num_threads(t);
        } else if (arg.rfind("--thread-scaling=", 0) == 0) {
            scaling_threads = parse_int_list(arg.substr(17));
        } else if (arg.find(".csv") != std::string::npos) {
            csv_out = arg;
        } else {
            int val = std::atoi(argv[i]);
            if (val > 0) iterations = val;
        }
    }

    std::cout << "============================================" << std::endl;
    std::cout << "  NYC Parking Violations - Query Benchmark" << std::endl;
    std::cout << "  (Phase 3: SoA + Parallel)" << std::endl;
    if (!scaling_threads.empty()) {
        std::cout << "  Thread scaling: ";
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

    ParkingAPI engine;

    std::cout << "Loading data..." << std::endl;
    auto tl0 = std::chrono::high_resolution_clock::now();
    size_t count = engine.load(filepath);
    auto tl1 = std::chrono::high_resolution_clock::now();
    double load_s = std::chrono::duration<double>(tl1 - tl0).count();

    std::cout << "Loaded " << count << " records in "
              << std::fixed << std::setprecision(1) << load_s << "s" << std::endl;
    std::cout << "Peak RSS: " << get_peak_rss_mb() << " MB\n" << std::endl;

    std::vector<BenchmarkEntry> results;

    auto run_filter = [&](const std::string& name, const std::string& desc, auto query_fn) {
        std::cout << "FILTER: " << desc << std::endl;

        auto warmup = query_fn();
        size_t matched = warmup.count();

        std::vector<double> times;
        for (int i = 0; i < iterations; i++) {
            auto t0 = std::chrono::high_resolution_clock::now();
            query_fn();
            auto t1 = std::chrono::high_resolution_clock::now();
            times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        auto stats = compute_stats(times);
        double selectivity = (100.0 * matched) / count;
        std::cout << "  -> " << matched << " matches ("
                  << std::fixed << std::setprecision(2) << selectivity << "% selectivity)" << std::endl;
        std::cout << "  -> " << stats.mean << " ms\n" << std::endl;

        BenchmarkEntry entry;
        entry.name = name;
        entry.time_stats = stats;
        entry.records_matched = matched;
        entry.records_scanned = count;
        entry.throughput = count / (stats.mean / 1000.0);
        results.push_back(entry);
    };

    auto run_agg = [&](const std::string& name, const std::string& desc, auto query_fn, bool show_top = false) {
        std::cout << "AGGREGATE: " << desc << std::endl;

        auto result = query_fn();
        size_t groups = result.counts.size();

        std::vector<double> times;
        for (int i = 0; i < iterations; i++) {
            auto t0 = std::chrono::high_resolution_clock::now();
            query_fn();
            auto t1 = std::chrono::high_resolution_clock::now();
            times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        auto stats = compute_stats(times);
        std::cout << "  -> " << groups << " groups found" << std::endl;

        if (show_top && result.counts.size() > 0) {
            // Sort by count descending
            auto sorted = result.counts;
            std::sort(sorted.begin(), sorted.end(),
                [](auto& a, auto& b) { return a.second > b.second; });

            std::cout << "  -> Top 3: ";
            for (size_t i = 0; i < std::min(size_t(3), sorted.size()); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << "#" << sorted[i].first << "=" << sorted[i].second;
            }
            std::cout << std::endl;
        }

        std::cout << "  -> " << std::fixed << std::setprecision(2) << stats.mean << " ms\n" << std::endl;

        BenchmarkEntry entry;
        entry.name = name;
        entry.time_stats = stats;
        entry.records_matched = groups;
        entry.records_scanned = count;
        entry.throughput = count / (stats.mean / 1000.0);
        results.push_back(entry);
    };

    // Run all queries
    auto run_all = [&]() {
        results.clear();

        std::cout << "=== SIMD-FRIENDLY QUERIES (pure reductions) ===\n" << std::endl;

        // SIMD Q1: Count in date range (no index collection)
        {
            std::cout << "SIMD: Count violations in 2024 (count only, no indices)" << std::endl;
            auto warmup = engine.count_in_date_range(20240101, 20241231);

            std::vector<double> times;
            for (int i = 0; i < iterations; i++) {
                auto t0 = std::chrono::high_resolution_clock::now();
                engine.count_in_date_range(20240101, 20241231);
                auto t1 = std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            }

            auto stats = compute_stats(times);
            std::cout << "  -> " << warmup.count << " violations counted" << std::endl;
            std::cout << "  -> " << std::fixed << std::setprecision(2) << stats.mean << " ms\n" << std::endl;

            BenchmarkEntry entry;
            entry.name = "simd_count";
            entry.time_stats = stats;
            entry.records_matched = warmup.count;
            entry.records_scanned = count;
            entry.throughput = count / (stats.mean / 1000.0);
            results.push_back(entry);
        }

        // SIMD Q2: Find min/max date (reduction)
        {
            std::cout << "SIMD: Find earliest and latest violation dates" << std::endl;
            auto warmup = engine.find_date_extremes();

            std::vector<double> times;
            for (int i = 0; i < iterations; i++) {
                auto t0 = std::chrono::high_resolution_clock::now();
                engine.find_date_extremes();
                auto t1 = std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            }

            auto stats = compute_stats(times);
            std::cout << "  -> min=" << warmup.min_date << ", max=" << warmup.max_date << std::endl;
            std::cout << "  -> " << std::fixed << std::setprecision(2) << stats.mean << " ms\n" << std::endl;

            BenchmarkEntry entry;
            entry.name = "simd_minmax";
            entry.time_stats = stats;
            entry.records_matched = 2;
            entry.records_scanned = count;
            entry.throughput = count / (stats.mean / 1000.0);
            results.push_back(entry);
        }

        std::cout << "=== FILTER QUERIES (collect indices) ===\n" << std::endl;

        run_filter("date_1month", "Violations in January 2024 (narrow range)",
            [&]() { return engine.find_by_date_range(20240101, 20240131); });

        run_filter("date_1year", "Violations in all of 2024 (wide range)",
            [&]() { return engine.find_by_date_range(20240101, 20241231); });

        run_filter("violation_code", "Double parking violations (code 46)",
            [&]() { return engine.find_by_violation_code(46); });

        run_filter("state_ny", "NY registered vehicles",
            [&]() { return engine.find_by_state(state_to_enum("NY", 2)); });

        run_filter("county_brooklyn", "Violations in Brooklyn",
            [&]() { return engine.find_by_county(county::BK); });

        std::cout << "=== AGGREGATE QUERIES ===\n" << std::endl;

        run_agg("group_precinct", "Count violations by precinct",
            [&]() { return engine.count_by_precinct(); }, true);

        run_agg("group_fiscal_year", "Count violations by fiscal year",
            [&]() { return engine.count_by_fiscal_year(); }, true);
    };

    // Thread scaling mode
    if (!scaling_threads.empty()) {
        for (int t : scaling_threads) {
            omp_set_num_threads(t);
            std::cout << "\n############################################" << std::endl;
            std::cout << "  THREAD COUNT: " << t << std::endl;
            std::cout << "############################################\n" << std::endl;

            run_all();

            // Print summary for this thread count
            std::cout << "============================================" << std::endl;
            std::cout << "  Summary (threads=" << t << ")" << std::endl;
            std::cout << "============================================" << std::endl;

            for (const auto& r : results) {
                std::cout << "  " << std::left << std::setw(18) << r.name
                          << std::right << std::fixed << std::setprecision(2)
                          << std::setw(8) << r.time_stats.mean << " ms"
                          << "  " << std::setprecision(0) << std::setw(12)
                          << r.throughput << " rec/s" << std::endl;
            }

            // CSV output per thread count
            if (!csv_out.empty()) {
                std::string per_csv = csv_out;
                size_t dot = per_csv.rfind(".csv");
                if (dot != std::string::npos) {
                    per_csv = per_csv.substr(0, dot) + "_t" + std::to_string(t) + ".csv";
                }
                std::ofstream ofs(per_csv);
                print_csv_header(ofs);
                for (auto& r : results) {
                    r.engine = "SoA_t" + std::to_string(t);
                    print_csv_row(r, ofs);
                }
                std::cout << "  Results written to: " << per_csv << std::endl;
            }
        }
        std::cout << "\nPeak RSS: " << get_peak_rss_mb() << " MB" << std::endl;
        std::cout << "Thread scaling study complete." << std::endl;
    } else {
        // Single run mode
        run_all();

        std::cout << "============================================" << std::endl;
        std::cout << "  Summary" << std::endl;
        std::cout << "============================================" << std::endl;

        for (const auto& r : results) {
            std::cout << "  " << std::left << std::setw(18) << r.name
                      << std::right << std::fixed << std::setprecision(2)
                      << std::setw(8) << r.time_stats.mean << " ms"
                      << "  " << std::setprecision(0) << std::setw(12)
                      << r.throughput << " rec/s" << std::endl;
        }

        if (!csv_out.empty()) {
            std::ofstream ofs(csv_out);
            print_csv_header(ofs);
            for (const auto& r : results) {
                print_csv_row(r, ofs);
            }
            std::cout << "Results written to: " << csv_out << std::endl;
        }

        std::cout << "\nPeak RSS: " << get_peak_rss_mb() << " MB" << std::endl;
        std::cout << "Done." << std::endl;
    }

    return 0;
}
