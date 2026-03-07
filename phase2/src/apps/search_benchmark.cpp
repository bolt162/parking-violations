#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <functional>
#include <omp.h>

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

/// Parse a comma-separated list of integers, e.g. "1,2,4,8,16"
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

/// Run all workloads at current OMP thread count, return results
static std::vector<benchmark::BenchmarkEntry> run_all_workloads(
    api::ParkingAPI& engine,
    const std::vector<Workload>& workloads,
    int iterations,
    const char* engine_name,
    int thread_count)
{
    std::vector<benchmark::BenchmarkEntry> results;

    for (const auto& w : workloads) {
        std::cout << "── " << w.id << ": " << w.description << " ──"
                  << std::endl;

        std::vector<double> times;
        size_t scanned = 0, matched = 0;

        if (w.is_aggregate) {
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
        entry.engine = std::string(engine_name) + "_t" + std::to_string(thread_count);
        entry.time_stats = stats;
        entry.records_scanned = scanned;
        entry.records_matched = matched;
        entry.throughput = throughput;
        results.push_back(entry);
    }

    return results;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <csv_file> [iterations] [output.csv] [--indexed] "
                  << "[--parallel] [--threads=N] [--thread-scaling=1,2,4,8]"
                  << std::endl;
        std::cerr << "  iterations:          timed runs per query (default: 12)"
                  << std::endl;
        std::cerr << "  output.csv:          write results to CSV file" << std::endl;
        std::cerr << "  --indexed:           use IndexedSearch" << std::endl;
        std::cerr << "  --parallel:          use ParallelSearch (OpenMP)" << std::endl;
        std::cerr << "  --threads=N:         set OpenMP thread count" << std::endl;
        std::cerr << "  --thread-scaling=..: load once, run at each thread count"
                  << std::endl;
        std::cerr << "                       e.g. --thread-scaling=1,2,4,6,8,10,12,14,16"
                  << std::endl;
        return 1;
    }

    const std::string filepath = argv[1];
    int iterations = 12;
    std::string csv_out;
    bool use_indexed = false;
    bool use_parallel = false;
    std::vector<int> scaling_threads;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--indexed") {
            use_indexed = true;
        } else if (arg == "--parallel") {
            use_parallel = true;
        } else if (arg.rfind("--threads=", 0) == 0) {
            int t = std::atoi(arg.c_str() + 10);
            if (t > 0) omp_set_num_threads(t);
        } else if (arg.rfind("--thread-scaling=", 0) == 0) {
            scaling_threads = parse_int_list(arg.substr(17));
            use_parallel = true;  // implied
        } else if (csv_out.empty() && arg.find(".csv") != std::string::npos) {
            csv_out = arg;
        } else {
            int val = std::atoi(argv[i]);
            if (val > 0) iterations = val;
        }
    }

    const char* engine_name = use_indexed ? "IndexedSearch"
                            : use_parallel ? "ParallelSearch"
                            : "LinearSearch";
    int num_threads = omp_get_max_threads();

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
    } else if (use_parallel) {
        std::cout << "  Threads: " << num_threads << std::endl;
    }
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ── Load data once ───────────────────────────────────────────────────

    api::ParkingAPI engine(use_indexed, use_parallel);

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

    workloads.push_back({"B1_date_narrow", "Date range 2024-01-01 to 2024-01-31",
        [](api::ParkingAPI& e) { return e.find_by_date_range(20240101, 20240131); },
        nullptr, false});

    workloads.push_back({"B2_date_wide", "Date range 2024-01-01 to 2024-12-31",
        [](api::ParkingAPI& e) { return e.find_by_date_range(20240101, 20241231); },
        nullptr, false});

    workloads.push_back({"B3_code_common", "Violation code 46 (double parking)",
        [](api::ParkingAPI& e) { return e.find_by_violation_code(46); },
        nullptr, false});

    workloads.push_back({"B4_code_rare", "Violation code 99 (rare)",
        [](api::ParkingAPI& e) { return e.find_by_violation_code(99); },
        nullptr, false});

    workloads.push_back({"B5_plate", "Plate lookup ('" + sample_plate + "')",
        [&sample_plate](api::ParkingAPI& e) {
            return e.find_by_plate(sample_plate.c_str());
        },
        nullptr, false});

    workloads.push_back({"B6_state_common", "State=NY (common)",
        [ny_enum](api::ParkingAPI& e) { return e.find_by_state(ny_enum); },
        nullptr, false});

    workloads.push_back({"B7_state_rare", "State=FL (rare)",
        [fl_enum](api::ParkingAPI& e) { return e.find_by_state(fl_enum); },
        nullptr, false});

    workloads.push_back({"B8_county", "County=BK (Brooklyn)",
        [bk_enum](api::ParkingAPI& e) { return e.find_by_county(bk_enum); },
        nullptr, false});

    workloads.push_back({"B9_precinct_agg", "Count by precinct",
        nullptr,
        [](api::ParkingAPI& e) { return e.count_by_precinct(); },
        true});

    workloads.push_back({"B10_fy_agg", "Count by fiscal year",
        nullptr,
        [](api::ParkingAPI& e) { return e.count_by_fiscal_year(); },
        true});

    // ── Thread-scaling mode ──────────────────────────────────────────────

    if (!scaling_threads.empty()) {
        // Run all workloads at each thread count, writing separate CSVs
        for (int t : scaling_threads) {
            omp_set_num_threads(t);
            std::cout << "\n############################################" << std::endl;
            std::cout << "  THREAD COUNT: " << t << std::endl;
            std::cout << "############################################\n" << std::endl;

            auto results = run_all_workloads(engine, workloads, iterations,
                                             engine_name, t);

            // Print summary for this thread count
            std::cout << "============================================" << std::endl;
            std::cout << "  Summary (" << engine_name << " t=" << t << ")"
                      << std::endl;
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

            // Write per-thread-count CSV
            if (!csv_out.empty()) {
                // Insert thread count before .csv extension
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
                for (const auto& r : results) {
                    benchmark::print_csv_row(r, ofs);
                }
                std::cout << "  Results written to: " << per_csv << std::endl;
            }
        }

        std::cout << "\n  Peak RSS: " << benchmark::get_peak_rss_mb() << " MB"
                  << std::endl;
        std::cout << "\nThread scaling study complete." << std::endl;
        return 0;
    }

    // ── Single-run mode (original behavior) ──────────────────────────────

    auto results = run_all_workloads(engine, workloads, iterations,
                                     engine_name, num_threads);

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
