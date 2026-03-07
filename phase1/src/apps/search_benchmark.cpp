#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <functional>

#include "api/parking_api.hpp"
#include "core/violation_record.hpp"
#include "core/field_types.hpp"
#include "core/text_ref.hpp"
#include "data/text_pool.hpp"
#include "benchmark_harness.hpp"

using namespace parking;

/// Describes a single search benchmark workload.
struct Workload {
    std::string id;
    std::string description;
    std::function<search::SearchResult(api::ParkingAPI&)>   search_fn;
    std::function<search::AggregateResult(api::ParkingAPI&)> agg_fn;
    bool is_aggregate;
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <csv_file> [iterations] [output.csv] [--indexed]"
                  << std::endl;
        std::cerr << "  iterations: number of timed runs per query (default: 12)"
                  << std::endl;
        std::cerr << "  output.csv: write results to CSV file" << std::endl;
        std::cerr << "  --indexed:  use IndexedSearch instead of LinearSearch"
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

    // ── Load data once ───────────────────────────────────────────────────

    api::ParkingAPI engine(use_indexed);

    std::cout << "Loading data..." << std::endl;
    auto t0 = std::chrono::high_resolution_clock::now();
    size_t count = engine.load(filepath);
    auto t1 = std::chrono::high_resolution_clock::now();
    double load_s = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "  Loaded " << count << " records in "
              << std::fixed << std::setprecision(1) << load_s << "s"
              << std::endl;
    std::cout << "  Peak RSS: " << benchmark::get_peak_rss_mb() << " MB\n"
              << std::endl;

    // Grab a plate from the first record for B5
    const auto& r0 = engine.record(0);
    const auto& pool = engine.text_pool();
    uint8_t plate_len = r0.str_lengths[core::SF_PLATE_ID];
    std::string sample_plate(pool.get(r0.str_offsets[core::SF_PLATE_ID]), plate_len);

    // Enum values for search
    uint8_t ny_enum = core::state_to_enum("NY", 2);
    uint8_t fl_enum = core::state_to_enum("FL", 2);
    uint8_t bk_enum = core::county_to_enum("BK", 2);

    // ── Define workloads ─────────────────────────────────────────────────

    std::vector<Workload> workloads;

    // B1: Date range narrow (Jan 2024)
    workloads.push_back({"B1_date_narrow", "Date range 2024-01-01 to 2024-01-31",
        [](api::ParkingAPI& e) { return e.find_by_date_range(20240101, 20240131); },
        nullptr, false});

    // B2: Date range wide (all 2024)
    workloads.push_back({"B2_date_wide", "Date range 2024-01-01 to 2024-12-31",
        [](api::ParkingAPI& e) { return e.find_by_date_range(20240101, 20241231); },
        nullptr, false});

    // B3: Violation code 46 (common — double parking)
    workloads.push_back({"B3_code_common", "Violation code 46 (double parking)",
        [](api::ParkingAPI& e) { return e.find_by_violation_code(46); },
        nullptr, false});

    // B4: Violation code 99 (rare)
    workloads.push_back({"B4_code_rare", "Violation code 99 (rare)",
        [](api::ParkingAPI& e) { return e.find_by_violation_code(99); },
        nullptr, false});

    // B5: Plate lookup (first record's plate)
    workloads.push_back({"B5_plate", "Plate lookup ('" + sample_plate + "')",
        [&sample_plate](api::ParkingAPI& e) {
            return e.find_by_plate(sample_plate.c_str());
        },
        nullptr, false});

    // B6: State filter NY (common)
    workloads.push_back({"B6_state_common", "State=NY (common)",
        [ny_enum](api::ParkingAPI& e) { return e.find_by_state(ny_enum); },
        nullptr, false});

    // B7: State filter FL (rare)
    workloads.push_back({"B7_state_rare", "State=FL (rare)",
        [fl_enum](api::ParkingAPI& e) { return e.find_by_state(fl_enum); },
        nullptr, false});

    // B8: County filter BK (Brooklyn)
    workloads.push_back({"B8_county", "County=BK (Brooklyn)",
        [bk_enum](api::ParkingAPI& e) { return e.find_by_county(bk_enum); },
        nullptr, false});

    // B9: Count by precinct (aggregate)
    workloads.push_back({"B9_precinct_agg", "Count by precinct",
        nullptr,
        [](api::ParkingAPI& e) { return e.count_by_precinct(); },
        true});

    // B10: Count by fiscal year (aggregate)
    workloads.push_back({"B10_fy_agg", "Count by fiscal year",
        nullptr,
        [](api::ParkingAPI& e) { return e.count_by_fiscal_year(); },
        true});

    // ── Run benchmarks ───────────────────────────────────────────────────

    std::vector<benchmark::BenchmarkEntry> results;

    for (const auto& w : workloads) {
        std::cout << "── " << w.id << ": " << w.description << " ──"
                  << std::endl;

        std::vector<double> times;
        size_t scanned = 0, matched = 0;

        if (w.is_aggregate) {
            // Warmup
            auto warmup_res = w.agg_fn(engine);
            scanned = warmup_res.total_scanned;
            matched = warmup_res.counts.size();

            times.reserve(iterations);
            for (int i = 0; i < iterations; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                auto res = w.agg_fn(engine);
                auto t1 = std::chrono::high_resolution_clock::now();
                double ms = std::chrono::duration<double, std::milli>(
                    t1 - t0).count();
                times.push_back(ms);
            }
        } else {
            // Warmup
            auto warmup_res = w.search_fn(engine);
            scanned = warmup_res.total_scanned;
            matched = warmup_res.count();

            times.reserve(iterations);
            for (int i = 0; i < iterations; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                auto res = w.search_fn(engine);
                auto t1 = std::chrono::high_resolution_clock::now();
                double ms = std::chrono::duration<double, std::milli>(
                    t1 - t0).count();
                times.push_back(ms);
            }
        }

        auto stats = benchmark::compute_stats(times);
        double throughput = scanned / (stats.mean / 1000.0);

        benchmark::print_stats(w.id, stats);
        std::cout << "    Scanned: " << scanned
                  << "  Matched: " << matched
                  << "  Throughput: " << std::fixed << std::setprecision(0)
                  << throughput << " rec/s" << std::endl;
        std::cout << std::endl;

        benchmark::BenchmarkEntry entry;
        entry.name = w.id;
        entry.engine = engine_name;
        entry.time_stats = stats;
        entry.records_scanned = scanned;
        entry.records_matched = matched;
        entry.throughput = throughput;
        results.push_back(entry);
    }

    // ── Summary ──────────────────────────────────────────────────────────

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

    // ── CSV output ───────────────────────────────────────────────────────

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
