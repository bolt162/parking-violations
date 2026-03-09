#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>
#include "parking.hpp"
#include "record.hpp"
#include "benchmark_harness.hpp"

using namespace parking;
using namespace parking::benchmark;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file> [iterations]" << std::endl;
        return 1;
    }

    std::string filepath = argv[1];
    int iterations = (argc >= 3) ? std::atoi(argv[2]) : 12;

    std::cout << "============================================" << std::endl;
    std::cout << "  NYC Parking Violations - Query Benchmark" << std::endl;
    std::cout << "  (Phase 3: SoA + Parallel)" << std::endl;
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
        entry.records_matched = 2;  // min and max
        entry.records_scanned = count;
        entry.throughput = count / (stats.mean / 1000.0);
        results.push_back(entry);
    }

    std::cout << "=== FILTER QUERIES (collect indices) ===\n" << std::endl;

    // Q1: Narrow date range (low selectivity)
    run_filter("date_1month",
        "Violations in January 2024 (narrow range)",
        [&]() { return engine.find_by_date_range(20240101, 20240131); });

    // Q2: Wide date range (high selectivity)
    run_filter("date_1year",
        "Violations in all of 2024 (wide range)",
        [&]() { return engine.find_by_date_range(20240101, 20241231); });

    // Q3: Common violation code
    run_filter("violation_code",
        "Double parking violations (code 46)",
        [&]() { return engine.find_by_violation_code(46); });

    // Q4: State filter
    run_filter("state_ny",
        "NY registered vehicles",
        [&]() { return engine.find_by_state(state_to_enum("NY", 2)); });

    // Q5: Borough filter
    run_filter("county_brooklyn",
        "Violations in Brooklyn",
        [&]() { return engine.find_by_county(county::BK); });

    std::cout << "=== AGGREGATE QUERIES ===\n" << std::endl;

    // Q6: Group by precinct (shows distribution)
    run_agg("group_precinct",
        "Count violations by precinct",
        [&]() { return engine.count_by_precinct(); }, true);

    // Q7: Group by fiscal year (shows trend)
    run_agg("group_fiscal_year",
        "Count violations by fiscal year",
        [&]() { return engine.count_by_fiscal_year(); }, true);

    // Summary
    std::cout << "============================================" << std::endl;
    std::cout << "  Summary" << std::endl;
    std::cout << "============================================" << std::endl;

    for (const auto& r : results) {
        std::cout << "  " << std::left << std::setw(18) << r.name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(8) << r.time_stats.mean << " ms"
                  << "  " << std::setprecision(0) << std::setw(12)
                  << r.throughput << " rec/s"
                  << std::endl;
    }

    std::cout << "\nPeak RSS: " << get_peak_rss_mb() << " MB" << std::endl;
    std::cout << "Done." << std::endl;

    return 0;
}
