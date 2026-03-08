#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <omp.h>

#include "parking.hpp"
#include "record.hpp"
#include "benchmark_harness.hpp"

using namespace parking;

// Parse a comma-separated list of integers
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

// Run load benchmark at a given thread count, return stats
static benchmark::Stats run_load_at_threads(
    const std::string& filepath, int iterations, int thread_count,
    size_t& record_count_out)
{
    omp_set_num_threads(thread_count);

    std::cout << "\n############################################" << std::endl;
    std::cout << "  THREAD COUNT: " << thread_count << std::endl;
    std::cout << "############################################\n" << std::endl;

    // Warmup
    {
        std::cout << "  Warmup..." << std::endl;
        ParkingAPI engine;
        record_count_out = engine.load(filepath);
        std::cout << "  Records: " << record_count_out << std::endl;
    }

    // Timed runs
    std::vector<double> load_times;
    load_times.reserve(iterations);

    std::cout << "  Running " << iterations << " timed iterations..."
              << std::endl;

    for (int i = 0; i < iterations; ++i) {
        ParkingAPI engine;

        auto t0 = std::chrono::high_resolution_clock::now();
        engine.load(filepath);
        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        load_times.push_back(ms);

        double throughput = record_count_out / (ms / 1000.0);
        std::cout << "    Run " << (i + 1) << "/" << iterations
                  << ": " << std::fixed << std::setprecision(1) << ms << " ms"
                  << " (" << std::setprecision(0) << throughput << " rec/s)"
                  << std::endl;
    }

    auto stats = benchmark::compute_stats(load_times);
    benchmark::print_stats("Full load", stats);
    return stats;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <csv_file> [iterations] [output.csv] [--threads=N] "
                  << "[--thread-scaling=1,4,8,16]"
                  << std::endl;
        std::cerr << "  iterations:          timed runs (default: 12)" << std::endl;
        std::cerr << "  output.csv:          write results to CSV file" << std::endl;
        std::cerr << "  --threads=N:         set OMP thread count (default: 6)" << std::endl;
        std::cerr << "  --thread-scaling=..: run at each thread count sequentially"
                  << std::endl;
        return 1;
    }

    // Default to 6 threads (optimal per benchmarks)
    omp_set_num_threads(6);

    const std::string filepath = argv[1];
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
        } else if (csv_out.empty() && arg.find(".csv") != std::string::npos) {
            csv_out = arg;
        } else {
            int val = std::atoi(argv[i]);
            if (val > 0) iterations = val;
        }
    }

    std::cout << "============================================" << std::endl;
    std::cout << "  Load Benchmark" << std::endl;
    std::cout << "  File: " << filepath << std::endl;
    std::cout << "  Iterations: " << iterations << std::endl;
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
    std::cout << "  sizeof(ViolationRecord) = "
              << sizeof(ViolationRecord) << " bytes" << std::endl;
    std::cout << "============================================\n" << std::endl;

    if (iterations < 1) {
        std::cerr << "Iterations must be >= 1" << std::endl;
        return 1;
    }

    // -- Thread-scaling mode --

    if (!scaling_threads.empty()) {
        for (int t : scaling_threads) {
            size_t record_count = 0;
            auto stats = run_load_at_threads(filepath, iterations, t,
                                             record_count);

            double mean_throughput = record_count / (stats.mean / 1000.0);

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

                benchmark::BenchmarkEntry entry;
                entry.name = "full_load";
                entry.engine = "parallel_t" + std::to_string(t);
                entry.time_stats = stats;
                entry.records_scanned = record_count;
                entry.records_matched = record_count;
                entry.throughput = mean_throughput;
                benchmark::print_csv_row(entry, ofs);

                std::cout << "  Results written to: " << per_csv << std::endl;
            }
        }

        std::cout << "\n  Peak RSS: " << benchmark::get_peak_rss_mb() << " MB"
                  << std::endl;
        std::cout << "\nLoad thread scaling study complete." << std::endl;
        return 0;
    }

    // -- Single-run mode --

    std::vector<double> load_times;
    load_times.reserve(iterations);

    size_t record_count = 0;

    // Warmup run
    {
        std::cout << "Warmup run..." << std::endl;
        ParkingAPI engine;
        record_count = engine.load(filepath);
        std::cout << "  Records: " << record_count << std::endl;
        std::cout << "  Peak RSS after warmup: "
                  << benchmark::get_peak_rss_mb() << " MB\n" << std::endl;
    }

    // Timed runs
    std::cout << "Running " << iterations << " timed iterations..." << std::endl;

    for (int i = 0; i < iterations; ++i) {
        ParkingAPI engine;

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

    // -- Results --

    std::cout << std::endl;
    auto stats = benchmark::compute_stats(load_times);

    std::cout << "-- Load Benchmark Results --" << std::endl;
    benchmark::print_stats("Full load", stats);
    std::cout << std::endl;

    double mean_throughput = record_count / (stats.mean / 1000.0);
    double mem_gb = (record_count * sizeof(ViolationRecord))
                    / (1024.0 * 1024.0 * 1024.0);

    std::cout << "  Records:           " << record_count << std::endl;
    std::cout << "  Struct size:       " << sizeof(ViolationRecord) << " bytes" << std::endl;
    std::cout << "  Mean throughput:   " << std::fixed << std::setprecision(0)
              << mean_throughput << " records/sec" << std::endl;
    std::cout << "  In-memory structs: " << std::setprecision(2)
              << mem_gb << " GB" << std::endl;
    std::cout << "  Peak RSS:          " << std::setprecision(1)
              << benchmark::get_peak_rss_mb() << " MB" << std::endl;

    // -- CSV output --

    if (!csv_out.empty()) {
        std::ofstream ofs(csv_out);
        benchmark::print_csv_header(ofs);

        benchmark::BenchmarkEntry entry;
        entry.name = "full_load";
        entry.engine = "parallel";
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
